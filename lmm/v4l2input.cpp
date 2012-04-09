#include "v4l2input.h"
#include "rawbuffer.h"
#include "lmmthread.h"
#include "emdesk/debug.h"

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

#include <QSemaphore>
#include <QTimer>
#include <QTime>

extern "C" {
	#include <linux/videodev2.h>
}

class captureThread : public LmmThread
{
public:
	captureThread(V4l2Input *parent)
		: LmmThread("CaptureThread")
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
	}
	int operation()
	{
		if (v4l2->captureLoop())
			return -1;
		return 0;
	}

private:
	V4l2Input *v4l2;
	QList<v4l2_buffer *> buffers;
};

V4l2Input::V4l2Input(QObject *parent) :
	BaseLmmElement(parent)
{
	captureWidth = 720;
	captureHeight = 480;
	deviceName = "/dev/video0";
	fd = -1;
	inputIndex = 0;
}

int V4l2Input::start()
{
	if (fd < 0) {
		int err = openCamera();
		if (err)
			return err;
		cThread = new captureThread(this);
		cThread->start();
		timing.start();
		return BaseLmmElement::start();
	}
	return 0;
}

int V4l2Input::stop()
{
	cThread->stop();
	cThread->wait();
	cThread->deleteLater();
	closeCamera();
	return BaseLmmElement::stop();
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
		mDebug("error in ioctl VIDIOC_REQBUFS");
	}
	for (i = 0; i < 3; i++)
		munmap(userptr[i], v4l2buf[i]->length);

	stopStreaming();

	qDeleteAll(v4l2buf);
	v4l2buf.clear();
	userptr.clear();
	close(fd);
	fd = -1;

	return 0;
}

int V4l2Input::openCamera()
{
	v4l2_std_id std_id = V4L2_STD_PAL_B;
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
	startStreaming();

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

	if (-1 == ioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		mDebug("Unable to get crop capabilities: %s", strerror(errno));
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
		mDebug("VIDIOC_DQBUF failed with err %d", errno);
		return NULL;
	}

	return v4l2buf[buffer.index];
}

void V4l2Input::aboutDeleteBuffer(const QMap<QString, QVariant> &params)
{
	v4l2_buffer *buffer = (v4l2_buffer *)params["v4l2Buffer"].value<void *>();
	mInfo("buffer %p", buffer);
	finishedLock.lock();
	finishedBuffers << buffer;
	finishedLock.unlock();
}

bool V4l2Input::captureLoop()
{
	while (finishedBuffers.size())
		putFrame(finishedBuffers.takeFirst());
	struct v4l2_buffer *buffer = getFrame();
	if (buffer) {
		mInfo("new frame %p", buffer);
		unsigned char *data = (unsigned char *)userptr[buffer->index];
		RawBuffer newbuf = RawBuffer();
		newbuf.setParentElement(this);

		newbuf.setRefData(data, buffer->length);
		newbuf.addBufferParameter("width", (int)captureWidth);
		newbuf.addBufferParameter("height", (int)captureHeight);
		newbuf.addBufferParameter("v4l2Buffer",
								   qVariantFromValue((void *)buffer));
		outputLock.lock();
		outputBuffers << newbuf;
		outputLock.unlock();
	}
	return false;
}

int V4l2Input::openDeviceNode()
{
	/* Open video capture device */
	fd = open(qPrintable(deviceName), O_RDWR, 0);
	if (fd == -1) {
		mDebug("Cannot open capture device %s", qPrintable(deviceName));
		return -ENODEV;
	}
	return 0;
}

int V4l2Input::enumInput(v4l2_input *input)
{
	if (ioctl(fd, VIDIOC_ENUMINPUT, input) == -1) {
		mDebug("Unable to enumerate input");
		return -EINVAL;
	} else
		mDebug("Input supports: 0x%llx", input->std);
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
	return 0;
}

int V4l2Input::setFormat(unsigned int chromaFormat, int width, int height)
{
	struct v4l2_format fmt;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		mDebug("Unable to get VIDIOC_G_FMT");
		return -EINVAL;
	}
	fmt.fmt.pix.width        = width;
	fmt.fmt.pix.height       = height;
	fmt.fmt.pix.bytesperline = width * 2;
	fmt.fmt.pix.pixelformat  = chromaFormat;
	fmt.fmt.pix.field        = V4L2_FIELD_NONE;
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
		mDebug("Unable to set VIDIOC_S_FMT");
		return -EINVAL;
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
		mDebug("VIDIOC_STREAMOFF failed on device");
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

