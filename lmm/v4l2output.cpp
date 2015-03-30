#include "v4l2output.h"
#include "tools/videoutils.h"

#include "debug.h"

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
#include <linux/videodev2.h>

V4l2Output::V4l2Output(QObject *parent) :
	BaseLmmOutput(parent)
{
	fd = -1;
	deviceName = "/dev/video2";
	bufferCount = 5;
}

int V4l2Output::outputBuffer(RawBuffer buf)
{
	if (driverStarted) {
		mInfo("receiving one from the driver");
		v4l2_buffer v4l2buf;
		v4l2buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		v4l2buf.memory = V4L2_MEMORY_USERPTR;
		if (ioctl(fd, VIDIOC_DQBUF, &v4l2buf) == -1) {
			mDebug("cannot receive buffer from driver");
			return -errno;
		}
		mInfo("buffer received from driver");
		buffersInUse.remove(v4l2buf.index);
		reqBuffers << reqBuffersInUse.take(v4l2buf.index);
	}

	v4l2_buffer *outbuf = reqBuffers.takeFirst();
	outbuf->m.userptr = (long unsigned int)buf.constData();
	if (ioctl(fd, VIDIOC_QBUF, outbuf)) {
		mDebug("Unable to queue buffer");
		return -EINVAL;
	}
	mInfo("buffer sent to driver");
	buffersInUse.insert(outbuf->index, buf);
	reqBuffersInUse.insert(outbuf->index, outbuf);

	if (!driverStarted && buffersInUse.size() == bufferCount) {
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
			mDebug("cannot start streaming in driver");
			return -errno;
		}
		mDebug("driver streamoned");
		driverStarted = true;
	}

	return 0;
}

int V4l2Output::start()
{
	driverStarted = false;
	int w, h;
	w = getParameter("videoWidth").toInt();
	h = getParameter("videoHeight").toInt();
	if (!w)
		w = 1280;
	if (!h)
		h = 720;

	dispWidth = w;
	dispHeight = h;
	if (fd < 0) {
		mInfo("opening display device: width=%d height=%d", w, h);
		int err = openDisplay();
		if (err)
			return err;
		return BaseLmmElement::start();
	}

	return 0;
}

int V4l2Output::stop()
{
	return BaseLmmOutput::stop();
}

int V4l2Output::openDisplay()
{
	int err = openDeviceNode();
	if (err)
		return err;

	struct v4l2_format fmt;
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		mDebug("Failed to determine video display format");
		return -ENOENT;
	}
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
	fmt.fmt.pix.width = dispWidth;
	fmt.fmt.pix.height = dispHeight;
	fmt.fmt.pix.bytesperline = VideoUtils::getLineLength(V4L2_PIX_FMT_NV12, dispWidth);
	fmt.fmt.pix.sizeimage = VideoUtils::getFrameSize(V4L2_PIX_FMT_NV12, dispWidth, dispHeight);
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
		mDebug("Failed VIDIOC_S_FMT on %s (%s)\n", qPrintable(deviceName),
				  strerror(errno));
		return errno;
	}

	struct v4l2_requestbuffers req;
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	req.count = bufferCount;
	req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	req.memory = V4L2_MEMORY_USERPTR;
	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
		mDebug("Could not allocate video display buffers");
		return -ENOMEM;
	}
	/* The driver may return less buffers than requested */
	if (req.count < (uint)bufferCount || !req.count) {
		mDebug("Insufficient device driver buffer memory");
		return -ENOMEM;
	}
	for (uint i = 0; i < req.count; i++) {
		reqBuffers << new v4l2_buffer;
		reqBuffers.last()->index = i;
		reqBuffers.last()->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		reqBuffers.last()->memory = V4L2_MEMORY_USERPTR;
		//reqBuffers.last()->m.userptr
		reqBuffers.last()->length = VideoUtils::getFrameSize(V4L2_PIX_FMT_NV12, dispWidth, dispHeight);
		reqBuffers.last()->flags = 0;
	}

	return 0;
}

int V4l2Output::openDeviceNode()
{
	/* Open video capture device */
	fd = open(qPrintable(deviceName), O_RDWR, 0);
	if (fd == -1) {
		mDebug("Cannot open capture device %s", qPrintable(deviceName));
		return -ENODEV;
	}
	return 0;
}
