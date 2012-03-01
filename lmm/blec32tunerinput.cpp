#include "blec32tunerinput.h"
#include "circularbuffer.h"
#include "streamtime.h"
#include "emdesk/debug.h"
#include "dvb/tsdemux.h"
#include "dvb/dvbutils.h"

#include <asm/errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>

#include <QTime>
#include <QStringList>

extern "C" {
	#include <linux/videodev2.h>
	#include "libavformat/avformat.h"
	#include "libavutil/avutil.h"
}

static URLProtocol *lmmUrlProtocol = NULL;
/* TODO: Fix single instance MpegTsDemux */
static Blec32TunerInput *demuxPriv = NULL;

static int lmmUrlOpen(URLContext *h, const char *url, int flags)
{
	h->priv_data = demuxPriv;
	return ((Blec32TunerInput *)h->priv_data)->openUrl(url, flags);
}

int lmmUrlRead(URLContext *h, unsigned char *buf, int size)
{
	return ((Blec32TunerInput *)h->priv_data)->readPacket(buf, size);
}

int lmmUrlWrite(URLContext *h, const unsigned char *buf, int size)
{
	(void)h;
	(void)buf;
	(void)size;
	return -EINVAL;
}

int64_t lmmUrlSeek(URLContext *h, int64_t pos, int whence)
{
	(void)h;
	(void)pos;
	(void)whence;
	return -EINVAL;
}

int lmmUrlClose(URLContext *h)
{
	return ((Blec32TunerInput *)h->priv_data)->closeUrl(h);
}

Blec32TunerInput::Blec32TunerInput(QObject *parent) :
	V4l2Input(parent)
{
	mpegtsraw = av_iformat_next(NULL);
	while (mpegtsraw) {
		if (strcmp(mpegtsraw->name, "mpegtsraw") == 0)
			break;
		mpegtsraw = av_iformat_next(mpegtsraw);
	}
	if (!lmmUrlProtocol) {
		lmmUrlProtocol = new URLProtocol;
		memset(lmmUrlProtocol , 0 , sizeof(URLProtocol));
		lmmUrlProtocol->name = "lmm";
		lmmUrlProtocol->url_open = lmmUrlOpen;
		lmmUrlProtocol->url_read = lmmUrlRead;
		lmmUrlProtocol->url_write = lmmUrlWrite;
		lmmUrlProtocol->url_seek = lmmUrlSeek;
		lmmUrlProtocol->url_close = lmmUrlClose;
		av_register_protocol2 (lmmUrlProtocol, sizeof (URLProtocol));
		demuxPriv = this;
	}

	captureWidth = 102;
	captureHeight = 480;
	inputIndex = 1;

	circBuf = new CircularBuffer(1024 * 1024 * 1, this);
}

int Blec32TunerInput::readPacket(uint8_t *buf, int buf_size)
{
	/* This routine may be called before the stream started */
	if (fd < 0)
		start();
	QTime timeout; timeout.start();
	while (buf_size > circBuf->usedSize()) {
		/* wait data to become availabe in circBuf */
		usleep(50000);
		if (timeout.elapsed() > 10000)
			return -ENOENT;
	}
	circBuf->lock();
	memcpy(buf, circBuf->getDataPointer(), buf_size);
	circBuf->useData(buf_size);
	circBuf->unlock();
	mInfo("read %d bytes into ffmpeg buffer", buf_size);
	return buf_size;
}

int Blec32TunerInput::openUrl(QString url, int)
{
	url.remove("lmm://");
	QStringList fields = url.split(":");
	QString stream = "tv";
	QString channel;
	if (fields.size() == 2) {
		stream = fields[0];
		channel = fields[1];
	} else
		channel = fields[0];
	if (!DVBUtils::tuneToChannel(channel))
		return -EINVAL;
	struct dvb_ch_info info = DVBUtils::currentChannelInfo();
	apid = info.apid;
	vpid = info.vpid;
	sid = info.spid;
	if (sid == 0)
		sid = 1;
	if (info.freq == 11981)
		sid = 2;
	pmt = -1;
	pcr = -1;
	return 0;
}

int Blec32TunerInput::closeUrl(URLContext *)
{
	/* no need to do anything, stream will be closed later */
	return 0;
}

int Blec32TunerInput::start()
{
	return V4l2Input::start();
}

int Blec32TunerInput::stop()
{
	circBuf->reset();
	return V4l2Input::stop();
}

bool Blec32TunerInput::captureLoop()
{
	struct v4l2_buffer *buffer = getFrame();
	if (buffer) {
		unsigned char *data = (unsigned char *)userptr[buffer->index];
		for (int i = 0; i < (int)buffer->length; i += 224) {
			if (data[i] != 0x47) {
				mDebug("sync error expected 0x47 got 0x%x", data[i]);
				continue;
			}
			int pid = data[i + 2] + ((data[i + 1] & 0x1f) << 8);
			if (pid == pcr)
				setSystemClock(tsDemux::parsePcr(&data[i]));
			if (pid == 0 && pmt < 1)
				pmt = tsDemux::findPmt(&data[i], sid);
			if (pcr < 0 && pmt > 0 && pid == pmt)
				pcr = tsDemux::findPcr(&data[i], pmt);
			if (pid != vpid && pid != apid && pid != pmt && pid != pcr && pid > 32)
				continue;
			circBuf->lock();
			if (circBuf->addData(&data[i], 188)) {
				mDebug("no space left on the circular buffer");
				circBuf->unlock();
				break;
			}
			circBuf->unlock();
		}
		putFrame(buffer);
	}
	return false;
}

#define V4L2_STD_525P_60 ((v4l2_std_id)(0x0001000000000000ULL))
int Blec32TunerInput::openCamera()
{
	v4l2_std_id std_id = V4L2_STD_525P_60;
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_input input;
	enum v4l2_buf_type type;
	int width = captureWidth, height = captureHeight;
	int err = 0;
	/* Open video capture device */
	fd = open(qPrintable(deviceName), O_RDWR, 0);
	if (fd == -1) {
		mDebug("Cannot open capture device %s", qPrintable(deviceName));
		return -ENODEV;
	}
	err = inputIndex;
	if (ioctl(fd, VIDIOC_S_INPUT, &err) == -1) {
		mDebug("Failed to set video input to %d, err is %d", inputIndex, errno);
		err = ENODEV;
		goto cleanup_devnode;
	}
	input.index = inputIndex;
	if (ioctl(fd, VIDIOC_ENUMINPUT, &input) == -1) {
		mDebug("Unable to enumerate input");
	} else
		mDebug("Input supports: 0x%llx", input.std);
	if (ioctl(fd, VIDIOC_S_STD, &std_id) == -1) {
		mDebug("Unable to set video standard");
		err = EINVAL;
		goto cleanup_devnode;
	}
	/* Query for capture device capabilities */
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
		mDebug("Unable to query device for capture capabilities");
		err = errno;
		goto cleanup_devnode;
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		mDebug("Device does not support capturing");
		err = EINVAL;
		goto cleanup_devnode;
	}
	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		mDebug("Device does not support streaming");
		err = EINVAL;
		goto cleanup_devnode;
	}
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		mDebug("Unable to get VIDIOC_G_FMT");
		err = EINVAL;
		goto cleanup_devnode;
	}
	/*case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGRBG10:*/
	fmt.fmt.pix.width        = width;
	fmt.fmt.pix.height       = height;
	fmt.fmt.pix.bytesperline = width * 2;
	fmt.fmt.pix.pixelformat  = V4L2_PIX_FMT_SBGGR16;
	fmt.fmt.pix.field        = V4L2_FIELD_NONE;

	adjustCropping(width, height);

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
		mDebug("Unable to set VIDIOC_S_FMT");
		err = EINVAL;
		goto cleanup_devnode;
	}

	if (allocBuffers(3, V4L2_BUF_TYPE_VIDEO_CAPTURE) < 0) {
		mDebug("Unable to allocate capture driver buffers");
		err = ENOMEM;
		goto cleanup_devnode;
	}

	/* Start the video streaming */
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
		mDebug("VIDIOC_STREAMON failed on device");
		err = EPERM;
		goto cleanup_devnode;
	}

	return 0;

cleanup_devnode:
	close(fd);
	return -err;
}

int Blec32TunerInput::setSystemClock(qint64 time)
{
	if (streamTime && time > 0)
		streamTime->setCurrentTime(time);
	return 0;
}
