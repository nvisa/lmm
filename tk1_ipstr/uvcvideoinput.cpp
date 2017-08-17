#include "uvcvideoinput.h"

#include <lmm/debug.h>

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

extern "C" {
	#include <linux/videodev2.h>
}

UvcVideoInput::UvcVideoInput(QObject *parent)
	: V4l2Input(parent)
{
	deviceName = "/dev/video0";
}

int UvcVideoInput::openCamera()
{
	int err = openDeviceNode();
	if (err)
		return err;

	struct v4l2_capability cap;
	err = queryCapabilities(&cap);
	ffDebug() << err << captureWidth << captureHeight;

	struct v4l2_format fmt;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1) {
		mDebug("Unable to get VIDIOC_G_FMT");
		err = EINVAL;
		return err;
	}
	int width = captureWidth;
	int height = captureHeight;
	fmt.fmt.pix.width        = width;
	fmt.fmt.pix.height       = height;
	fmt.fmt.pix.bytesperline = width * 2;
	fmt.fmt.pix.pixelformat  = V4L2_PIX_FMT_UYVY;
	fmt.fmt.pix.field        = V4L2_FIELD_NONE;

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
		mDebug("Unable to set VIDIOC_S_FMT");
		err = EINVAL;
		return err;
	}

	if (allocBuffers(5, V4L2_BUF_TYPE_VIDEO_CAPTURE) < 0) {
		mDebug("Unable to allocate capture driver buffers");
		err = ENOMEM;
		return err;
	}

	qDebug() << startStreaming();

	return 0;
}

