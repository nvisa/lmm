#include "dm8168videocontroller.h"

#define __EXPORTED_HEADERS__
#include <linux/ti81xxfb.h>

#include <lmm/debug.h>

#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/errno.h>
#include <sys/ioctl.h>
#include <errno.h>

DM8168VideoController::DM8168VideoController(QObject *parent) :
	QObject(parent)
{
	dispCtrlNode.setFileName("/sys/devices/platform/vpss/graphics0/enabled");
	if (!dispCtrlNode.open(QIODevice::WriteOnly | QIODevice::Unbuffered))
		mDebug("unable to open display control sysfs node");
}

void DM8168VideoController::showVideo()
{
	dispCtrlNode.write("0\n");
}

void DM8168VideoController::showGraphics()
{
	dispCtrlNode.write("1\n");
}

int DM8168VideoController::useTransparencyKeying(bool enable, int key)
{
	int err = 0;
	int fd = open("/dev/fb0", O_RDWR);
	if(fd < 0) {
		mDebug("error openning framebuffer device");
		return -errno;
	}

	struct ti81xxfb_region_params pars;
	if (ioctl(fd, TIFB_GET_PARAMS, &pars) < 0) {
		mDebug("error getting tifb parameters");
		err = -errno;
		goto out;
	}
	pars.transen = TI81XXFB_FEATURE_ENABLE;
	if (!enable)
		pars.transen = TI81XXFB_FEATURE_DISABLE;
	pars.transcolor = key;
	pars.transtype = TI81XXFB_TRANSP_LSBMASK_NO;
	if (ioctl(fd, TIFB_SET_PARAMS, &pars) == -1) {
		mDebug("error enabling transparency keying");
		err = -errno;
		goto out;
	}
	mDebug("transparency keying adjusted successfully");
out:
	::close(fd);
	return err;
}
