#include "v4l2input.h"
#include "rawbuffer.h"
#include "lmmthread.h"
#include "debug.h"
#include "tools/videoutils.h"
#include "tools/unittimestat.h"

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>

#include <QTimer>

extern "C" {
	#include <linux/videodev2.h>
}

V4l2Input::V4l2Input(QObject *parent) :
	BaseLmmElement(parent)
{
	captureWidth = 720;
	captureHeight = 576;
	deviceName = "/dev/video0";
	fd = -1;
	inputIndex = 0;
	nonBlockingIO = true;
	manualStart = false;
	frameSkip = 0;
}

int V4l2Input::start()
{
	if (fd < 0) {
		int w, h;
		w = getParameter("videoWidth").toInt();
		h = getParameter("videoHeight").toInt();
		if (w)
			captureWidth = w;
		if (h)
			captureHeight = h;
		mInfo("opening camera");
		int err = openCamera();
		if (err)
			return err;
		timing.start();
		captureCount = 0;
		return BaseLmmElement::start();
	}
	return 0;
}

int V4l2Input::stop()
{
	int err = BaseLmmElement::stop();
	closeCamera();
	return err;
}

int V4l2Input::prepareStop()
{
	stopStreaming();
	return BaseLmmElement::prepareStop();
}

int V4l2Input::flush()
{
	return BaseLmmElement::flush();
}

int V4l2Input::closeCamera()
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int i;

	struct v4l2_requestbuffers req;
	req.count = 0;
	req.type = type;
	req.memory = V4L2_MEMORY_MMAP;
	/* Release buffers in the capture device driver */
	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
		mDebug("error %d in ioctl VIDIOC_REQBUFS", errno);
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
	v4l2_std_id std_id = V4L2_STD_PAL;
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_input input;
	int width = captureWidth, height = captureHeight;
	int err = 0;

	err = openDeviceNode();
	if (err)
		return err;

	enumInput(&input);
	err = setInput(&input);
	if (err)
		return err;

	err = setStandard(&std_id);
	if (err)
		return err;

	err = queryCapabilities(&cap);
	if (err)
		return err;

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		mDebug("Unable to get VIDIOC_G_FMT");
		err = EINVAL;
		goto cleanup_devnode;
	}
	fmt.fmt.pix.width        = width;
	fmt.fmt.pix.height       = height;
	fmt.fmt.pix.bytesperline = width * 2;
	fmt.fmt.pix.pixelformat  = V4L2_PIX_FMT_UYVY;
	fmt.fmt.pix.field        = V4L2_FIELD_INTERLACED;

	//adjustCropping(width, height);

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
		mDebug("Unable to set VIDIOC_S_FMT");
		err = EINVAL;
		goto cleanup_devnode;
	}

	if (allocBuffers(5, V4L2_BUF_TYPE_VIDEO_CAPTURE) < 0) {
		mDebug("Unable to allocate capture driver buffers");
		err = ENOMEM;
		goto cleanup_devnode;
	}

	if (manualStart)
		return 0;

	/* Start the video streaming */
	startStreaming();

	return 0;

cleanup_devnode:
	close(fd);
	return -err;
}

int V4l2Input::adjustCropping(int width, int height, int left, int top)
{
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;

	memset (&cropcap, 0, sizeof (cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == ioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		mDebug("Unable to get crop capabilities: %s", strerror(errno));
		return -1;
	}

	memset (&crop, 0, sizeof (crop));

	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	crop.c = cropcap.defrect;

	crop.c.width = width;
	crop.c.height = height;
	crop.c.left = left;
	crop.c.top = top;

	/* Ignore if cropping is not supported (EINVAL). */
	if (-1 == ioctl (fd, VIDIOC_S_CROP, &crop)
			&& errno != EINVAL) {
		mDebug("Unable to set crop parameters");
		return -errno;
	}

	mDebug("image cropped to %dx%d", width, height);
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
		if (errno != EAGAIN)
			mDebug("VIDIOC_DQBUF failed with err %d", errno);
		return NULL;
	}

	return v4l2buf[buffer.index];
}

void V4l2Input::aboutToDeleteBuffer(const RawBufferParameters *params)
{
	v4l2_buffer *buffer = (v4l2_buffer *)params->v4l2Buffer;
	mInfo("buffer %p", buffer);
	putFrame(buffer);
}

int V4l2Input::processBlocking(int ch)
{
	Q_UNUSED(ch);
	if (getState() == STOPPED)
		return -EINVAL;
	if (eofSent)
		return -ENODATA;
	struct v4l2_buffer *buffer = getFrame();
	if (getState() == STOPPED)
		return -EINVAL;
	if (buffer) {
		captureCount++;
		processTimeStat->startStat();
		int ret = 0;
		if (frameSkip) {
			if (frameSkipCount--) {
				putFrame(buffer);
				return ret;
			}
			frameSkipCount = frameSkip;
		}
		ret = processBuffer(buffer);
		processTimeStat->addStat();
		return ret;
	}

	usleep(10000);
	return nonBlockingIO ? 0 : -ENXIO;
}

/**
 * @brief V4l2Input::info
 * @return
 *
 * VIDIOC_QUERYCAP
 * VIDIOC_G_FMT(set?, try?)
 * VIDIOC_ENUM_FMT
 * VIDIOC_ENUMINPUT
 * VIDIOC_G_INPUT, (set?)
 */
int V4l2Input::info()
{
	int err = openDeviceNode();
	if (err)
		return err;

	/* query capabilities */
	struct v4l2_capability cap;
	err = queryCapabilities(&cap);
	if (err)
		return err;
	mDebug("device has streaming/capture capabilities.");

	struct v4l2_input input;
	input.type = V4L2_INPUT_TYPE_CAMERA;
	input.index = 0;
	err = enumInput(&input);
	while (!err) {
		mDebug("input %d is %s", input.index, input.name);

		/* enum format */
		int fmtIndex = 0;
		int tmp = enumFmt(fmtIndex);
		while (!tmp) {
			fmtIndex++;
			tmp = enumFmt(fmtIndex);
		}

		/* get format */
		struct v4l2_format fmt;
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
			mDebug("Unable to get VIDIOC_G_FMT");
			return -EINVAL;
		}
		int x = fmt.fmt.pix.pixelformat;
		mDebug("pix info: \nbytesperline=%d\n"
			   "field=%d\n"
			   "height=%d\n"
			   "width=%d\n"
			   "size=%d\n"
			   "pixfmt=%c%c%c%c"
			   , fmt.fmt.pix.bytesperline, fmt.fmt.pix.field, fmt.fmt.pix.height
			   , fmt.fmt.pix.width, fmt.fmt.pix.sizeimage, x >> 24, (x >> 16) & 0xff, (x >> 8) & 0xff, (x >> 0) & 0xff);

		break;
		input.index++;
		err = enumInput(&input);
	}

	if (ioctl(fd, VIDIOC_G_INPUT, &input.index)) {
		mDebug("Unable to get input from device");
		return -EINVAL;
	}
	mDebug("current selected input is %d", input.index);

	if (ioctl(fd, VIDIOC_S_INPUT, &input.index) == -1) {
		mDebug("Failed to set video input to %d, err is %d", inputIndex, errno);
		return -EINVAL;
	}

	queryStandard();

	close(fd);
	return 0;
}

int V4l2Input::setFrameSkip(int cnt)
{
	frameSkip = cnt - 1;
	frameSkipCount = 0;
	return 0;
}

int V4l2Input::processBuffer(v4l2_buffer *buffer)
{
	mInfo("new frame %p", buffer);
	unsigned char *data = (unsigned char *)userptr[buffer->index];
	RawBuffer newbuf = RawBuffer();
	newbuf.setParentElement(this);

	newbuf.setRefData("video/x-raw-yuv", data, buffer->length);
	newbuf.pars()->videoWidth = captureWidth;
	newbuf.pars()->videoHeight = captureHeight;
	newbuf.pars()->v4l2Buffer = (quintptr *)buffer;
	newbuf.pars()->v4l2PixelFormat = V4L2_PIX_FMT_UYVY;
	newOutputBuffer(0, newbuf);

	return 0;
}

int V4l2Input::openDeviceNode()
{
	/* Open video capture device */
	if (nonBlockingIO)
		fd = open(qPrintable(deviceName), O_RDWR | O_NONBLOCK, 0);
	else
		fd = open(qPrintable(deviceName), O_RDWR, 0);
	if (fd == -1) {
		mDebug("Cannot open capture device %s", qPrintable(deviceName));
		return -ENODEV;
	}
	return 0;
}

int V4l2Input::enumStd()
{
	v4l2_standard dstd;
	for (int i = 0;; i++) {
		v4l2_standard std;
		memset(&std, 0, sizeof(v4l2_standard));
		std.frameperiod.numerator = 1;
		std.frameperiod.denominator = 0;
		std.index = i;
		if (ioctl(fd, VIDIOC_ENUMSTD, &std) == -1) {
			if (errno == EINVAL || errno == ENOTTY) {
				dstd = std;
				break;
			}
			mDebug("Unable to enumerate standard");
			return -EINVAL;
		}
	}
	mDebug("Standard supported: %s fps=%d/%d", dstd.name, dstd.frameperiod.denominator, dstd.frameperiod.numerator);
	return 0;
}

int V4l2Input::enumFmt(int index)
{
	struct v4l2_fmtdesc desc;
	desc.index = index;
	desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_ENUM_FMT, &desc) == -1) {
		mDebug("end of format enumaration");
		return -EINVAL;
	}
	int x = desc.pixelformat;
	mDebug("pixfmt=%c%c%c%c", x >> 24, (x >> 16) & 0xff, (x >> 8) & 0xff, (x >> 0) & 0xff);
	return 0;
}

#define V4L2_STD_720P_30        ((v4l2_std_id)(0x0100000000000000ULL))
#define V4L2_STD_720P_60        ((v4l2_std_id)(0x0004000000000000ULL))
static const QList<QPair<quint64, QString > > getV4L2Standards()
{
	QList<QPair<quint64, QString> > list;
	list << QPair<quint64, QString>(V4L2_STD_NTSC, "NTSC");
	list << QPair<quint64, QString>(V4L2_STD_SECAM, "SECAM");
	list << QPair<quint64, QString>(V4L2_STD_PAL, "PAL");
	list << QPair<quint64, QString>(V4L2_STD_525_60, "525_60");
	list << QPair<quint64, QString>(V4L2_STD_625_50, "525_50");
	list << QPair<quint64, QString>(V4L2_STD_ATSC, "ATSC");
	list << QPair<quint64, QString>(V4L2_STD_720P_30, "720P_30");
	list << QPair<quint64, QString>(V4L2_STD_720P_60, "720P_60");
	return list;
}

int V4l2Input::enumInput(v4l2_input *input)
{
	if (ioctl(fd, VIDIOC_ENUMINPUT, input) == -1) {
		mDebug("Unable to enumerate input");
		return -EINVAL;
	} else {
		mDebug("Input %d supports: 0x%llx", input->index, input->std);
		const QList<QPair<quint64, QString > > list = getV4L2Standards();
		for(int i = 0; i < list.size(); i++) {
			if ((list[i].first & input->std) == list[i].first)
				mDebug("Input %d supports %s", input->index, qPrintable(list[i].second));
		}
	}
	return 0;
}

int V4l2Input::setInput(v4l2_input *input)
{
	if (ioctl(fd, VIDIOC_S_INPUT, input) == -1) {
		mDebug("Failed to set video input to %d, err is %d", inputIndex, errno);
		return -ENODEV;
	}
	return 0;
}

int V4l2Input::setStandard(v4l2_std_id *std_id)
{
	if (ioctl(fd, VIDIOC_S_STD, std_id) == -1) {
		mDebug("Unable to set video standard");
		return -EINVAL;
	}
	return 0;
}

int V4l2Input::queryCapabilities(v4l2_capability *cap)
{
	/* Query for capture device capabilities */
	if (ioctl(fd, VIDIOC_QUERYCAP, cap) == -1) {
		mDebug("Unable to query device for capture capabilities");
		return errno;
	}
	if (!(cap->capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		mDebug("Device does not support capturing");
		return -EINVAL;
	}
	if (!(cap->capabilities & V4L2_CAP_STREAMING)) {
		mDebug("Device does not support streaming");
		return -EINVAL;
	}
	return 0;
}

int V4l2Input::queryStandard()
{
	v4l2_std_id std;
	if (ioctl(fd, VIDIOC_QUERYSTD, &std) == -1) {
		mDebug("Unable to query input standard");
		return -EINVAL;
	}
	const QList<QPair<quint64, QString > > list = getV4L2Standards();
	for(int i = 0; i < list.size(); i++) {
		if ((list[i].first & std) == list[i].first)
			mDebug("Input supports %s", qPrintable(list[i].second));
	}
	return 0;
}

int V4l2Input::setFormat(unsigned int chromaFormat, int width, int height, bool interlaced)
{
	struct v4l2_format fmt;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		mDebug("Unable to get VIDIOC_G_FMT, error %d", errno);
		return -EINVAL;
	}
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width        = width;
	fmt.fmt.pix.height       = height;
	fmt.fmt.pix.bytesperline = VideoUtils::getLineLength(chromaFormat, width);
	fmt.fmt.pix.sizeimage = fmt.fmt.pix.bytesperline * height;
	fmt.fmt.pix.pixelformat  = chromaFormat;
	fmt.fmt.pix.field        = interlaced ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
		mDebug("Unable to set VIDIOC_S_FMT with error %d", errno);
		return -errno;
	}
	return 0;
}

int V4l2Input::startStreaming()
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
		mDebug("VIDIOC_STREAMON failed on device");
		return -EPERM;
	}
	return 0;
}

int V4l2Input::stopStreaming()
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	/* Stop the video streaming */
	if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
		mDebug("VIDIOC_STREAMOFF failed on device with err %d", errno);
		return -errno;
	}
	return 0;
}

/**
 * Gives previously received camera buffer back to V4L2 kernel driver.
 */
int V4l2Input::putFrame(struct v4l2_buffer *buffer)
{
	mInfo("giving back frame %p to driver", buffer);
	buffer->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/* Issue captured frame buffer back to device driver */
	if (ioctl(fd, VIDIOC_QBUF, buffer) == -1) {
		mDebug("VIDIOC_QBUF failed (%s)", strerror(errno));
		return -errno;
	}

	return 0;
}

