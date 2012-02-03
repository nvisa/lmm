#include "v4l2input.h"
#include "rawbuffer.h"
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

#include <QThread>
#include <QSemaphore>
#include <QTimer>
#include <QTime>

static URLProtocol *lmmUrlProtocol = NULL;
/* TODO: Fix single instance MpegTsDemux */
static V4l2Input *demuxPriv = NULL;

static int lmmUrlOpen(URLContext *h, const char *url, int flags)
{
	h->priv_data = demuxPriv;
	return ((V4l2Input *)h->priv_data)->openUrl(url, flags);
}

int lmmUrlRead(URLContext *h, unsigned char *buf, int size)
{
	return ((V4l2Input *)h->priv_data)->readPacket(buf, size);
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
	return ((V4l2Input *)h->priv_data)->closeUrl(h);
}

class captureThread : public QThread
{
public:
	captureThread(V4l2Input *parent)
	{
		v4l2 = parent;
	}
	struct v4l2_buffer * getNextBuffer()
	{
		if (buffers.size())
			return buffers.takeFirst();
		return NULL;
	}
	void releaseBuffer(struct v4l2_buffer *)
	{
		sem.release();
	}
	void stop()
	{
		exit = true;
		sem.release();
	}

	void run()
	{
		exit = false;
		while (!exit) {
			if (v4l2->captureLoop())
				break;
		}
	}
private:
	V4l2Input *v4l2;
	QList<v4l2_buffer *> buffers;
	QSemaphore sem;
	bool exit;
};

#define V4L2_STD_525P_60 ((v4l2_std_id)(0x0001000000000000ULL))

V4l2Input::V4l2Input(QObject *parent) :
	BaseLmmElement(parent)
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
	deviceName = "/dev/video0";
	fd = -1;
	inputIndex = 1;
	cThread = new captureThread(this);
	circBuf = new CircularBuffer(1024 * 1024 * 10, this);
}

int V4l2Input::readPacket(uint8_t *buf, int buf_size)
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

int V4l2Input::openUrl(QString url, int)
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
	pmt = -1;
	pcr = -1;
	return 0;
}

int V4l2Input::closeUrl(URLContext *)
{
	/* no need to do anything, stream will be closed later */
	return 0;
}

int V4l2Input::start()
{
	if (fd < 0) {
		int err = openCamera();
		if (err)
			return err;
		cThread->start();
	}
	return 0;
}

int V4l2Input::stop()
{
	cThread->stop();
	cThread->wait();
	circBuf->reset();
	return closeCamera();
}

int V4l2Input::closeCamera()
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int i;

	/* Stop the video streaming */
	if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
		mDebug("VIDIOC_STREAMOFF failed on device");
	}

	for (i = 0; i < 3; i++)
		munmap(userptr[i], v4l2buf[i]->length);

	qDeleteAll(v4l2buf);
	v4l2buf.clear();
	userptr.clear();
	close(fd);
	fd = -1;
	return 0;
}

int V4l2Input::openCamera()
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

int V4l2Input::adjustCropping(int width, int height)
{
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;

	memset (&cropcap, 0, sizeof (cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == ioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
		printf("Unable to get crop capabilities");
		return -1;
	}

	memset (&crop, 0, sizeof (crop));

	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	crop.c = cropcap.defrect;

	crop.c.width = width;
	crop.c.height = height;
	crop.c.left = 0;
	crop.c.top = 0;

	/* Ignore if cropping is not supported (EINVAL). */
	if (-1 == ioctl (fd, VIDIOC_S_CROP, &crop)
			&& errno != EINVAL) {
		mDebug("Unable to set crop parameters");
		return -errno;
	}

	return 0;
}

int V4l2Input::allocBuffers(unsigned int buf_cnt, enum v4l2_buf_type type)
{
	struct v4l2_requestbuffers req;
	struct v4l2_format fmt;

	fmt.type = type;
	unsigned int i;

	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		mDebug("error in ioctl VIDIOC_G_FMT");
		return -EINVAL;
	}

	req.count = buf_cnt;
	req.type = type;
	req.memory = V4L2_MEMORY_MMAP;

	/* Allocate buffers in the capture device driver */
	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
		mDebug("error in ioctl VIDIOC_REQBUFS");
		return -ENOMEM;
	}

	if (req.count < buf_cnt || !req.count) {
		mDebug("error in buffer count");
		return -ENOMEM;
	}

	for (i = 0; i < buf_cnt; i++) {
		v4l2buf << new struct v4l2_buffer;
		userptr << 0;
		v4l2buf[i]->type = type;
		v4l2buf[i]->memory = V4L2_MEMORY_MMAP;
		v4l2buf[i]->index = i;

		if (ioctl(fd, VIDIOC_QUERYBUF, v4l2buf[i]) == -1) {
			mDebug("error in ioctl VIDIOC_QUERYBUF");
			return -ENOMEM;
		}

		/* Map the driver buffer to user space */
		userptr[i] = (char *)mmap(NULL,
								  v4l2buf[i]->length,
								  PROT_READ | PROT_WRITE,
								  MAP_SHARED,
								  fd,
								  v4l2buf[i]->m.offset);

		if (userptr[i] == MAP_FAILED) {
			mDebug("error while memory mapping buffers");
			return -ENOMEM;
		}

		/* Queue buffer in device driver */
		if (ioctl(fd, VIDIOC_QBUF, v4l2buf[i]) == -1) {
			mDebug("error while queing buffers");
			return -ENOMEM;
		}
	}

	return 0;
}

v4l2_buffer *V4l2Input::getFrame()
{
	struct v4l2_buffer buffer;

	buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffer.memory = V4L2_MEMORY_MMAP;

	/* Get a frame buffer with captured data */
	if (ioctl(fd, VIDIOC_DQBUF, &buffer) < 0) {
		mDebug("VIDIOC_DQBUF failed with err %d", errno);
		return NULL;
	}

	return v4l2buf[buffer.index];
}

bool V4l2Input::captureLoop()
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
				pmt = tsDemux::findPmt(&data[i], 1);
			if (pcr < 0 && pmt > 0 && pid == pmt)
				pcr = tsDemux::findPcr(&data[i], pmt);
			if (pid != vpid && pid != apid && pid != pmt && pid != pcr && pid > 32)
				continue;
			circBuf->lock();
			if (circBuf->addData(&data[i], 188)) {
				qDebug("no space left on the circular buffer");
				circBuf->unlock();
				break;
			}
			circBuf->unlock();
		}
		putFrame(buffer);
	}
	return false;
}

int V4l2Input::setSystemClock(qint64 time)
{
	if (streamTime)
		streamTime->setCurrentTime(time);
	return 0;
}

/**
 * Gives previously received camera buffer back to V4L2 kernel driver.
 */
int V4l2Input::putFrame(struct v4l2_buffer *buffer)
{
	buffer->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/* Issue captured frame buffer back to device driver */
	if (ioctl(fd, VIDIOC_QBUF, buffer) == -1) {
		mDebug("VIDIOC_QBUF failed (%s)", strerror(errno));
		return -errno;
	}

	return 0;
}

