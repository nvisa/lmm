#include "zynqvideoinput.h"
#include "debug.h"
#include "drm.h"

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

#include <QSize>
#include <QFile>

extern "C" {
	#include <linux/videodev2.h>
	#include <linux/media.h>
}

ZynqVideoInput::ZynqVideoInput(QObject *parent) :
	V4l2Input(parent)
{
	captureWidth = 1280;
	captureHeight = 720;
	nonBlockingIO = false;
}

int ZynqVideoInput::openCamera()
{
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	int width = captureWidth, height = captureHeight;
	int err = 0;

	err = openDeviceNode();
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
	if (getControlValue("/dev/v4l-subdev1", V4L2_CID_TEST_PATTERN))
		fmt.fmt.pix.pixelformat  = V4L2_PIX_FMT_YUYV;
	else
		fmt.fmt.pix.pixelformat  = V4L2_PIX_FMT_UYVY;
	pixFmt = fmt.fmt.pix.pixelformat;
	fmt.fmt.pix.field        = V4L2_FIELD_NONE;

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

	if (manualStart)
		return 0;

	/* Start the video streaming */
	startStreaming();

	return 0;

cleanup_devnode:
	close(fd);
	return -err;
}

int ZynqVideoInput::closeCamera()
{
	close(fd);
	qDeleteAll(v4l2buf);
	v4l2buf.clear();
	userptr.clear();
	fd = -1;
	mInfo("capture closed");

	return 0;
}

int ZynqVideoInput::printMediaInfo(const QString &filename)
{
	int fd = open(qPrintable(filename), O_RDWR, 0);
	if (fd < 0)
		return -errno;
	struct media_device_info dinfo;
	if (ioctl(fd, MEDIA_IOC_DEVICE_INFO, &dinfo) == -1) {
		fDebug("error '%s' getting device info for '%s'", strerror(errno), qPrintable(filename));
		close(fd);
		return -errno;
	}
	fDebug("driver: %s", dinfo.driver);
	fDebug("bus: %s", dinfo.bus_info);
	fDebug("model: %s", dinfo.model);
	fDebug("serial: %s", dinfo.serial);

	struct media_entity_desc entity;
	memset(&entity, 0, sizeof(media_entity_desc));
	int ind = 0;
	while (1) {
		entity.id = ind++ | MEDIA_ENT_ID_FLAG_NEXT;
		if (ioctl(fd, MEDIA_IOC_ENUM_ENTITIES, &entity) < 0)
			break;
		qDebug("found media entity: %s(%d)", entity.name, entity.id);
	}

	for (int i = 0; i < INT_MAX; i++) {
		QString filename = QString("/dev/v4l-subdev%1").arg(i);
		if (!QFile::exists(filename))
			break;
		listControls(filename);
	}

	//qDebug() << setControlValue("/dev/v4l-subdev1", V4L2_CID_TEST_PATTERN, 0);
	qDebug() << "current test pattern:" << getControlValue("/dev/v4l-subdev0", V4L2_CID_TEST_PATTERN);

	close(fd);

	return 0;
}

QSize ZynqVideoInput::getSize(int ch)
{
	if (ch)
		return QSize(0, 0);
	return QSize(captureWidth, captureHeight);
}
