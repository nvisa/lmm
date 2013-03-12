#include "fboutput.h"
#define DEBUG
#include "debug.h"
#include "rawbuffer.h"
#include "streamtime.h"

#include "dmaidecoder.h"

#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

int FbOutput::openFb(QString filename)
{
	struct fb_var_screeninfo vScreenInfo;
	struct fb_fix_screeninfo fScreenInfo;
	fd = open(qPrintable(filename), O_RDWR);
	if(fd < 0)
		return -ENOENT;

	if (ioctl(fd, FBIOGET_FSCREENINFO, &fScreenInfo) == -1) {
		::close(fd);
		mDebug("failed to get fix screen info");
		return -EINVAL;
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &vScreenInfo) == -1) {
		::close(fd);
		mDebug("failed to get var screen info");
		return -EINVAL;
	}
	fbLineLen = fScreenInfo.line_length;
	fbHeight = vScreenInfo.yres;
	fbSize = fScreenInfo.line_length * vScreenInfo.yres;
	/* Map the attribute window to this user space process */
	fbAddr = (unsigned char *) mmap(NULL, fbSize,
										PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	for (int i = 0; i < fbSize; i += 2) {
		fbAddr[i] = 0x80;
		fbAddr[i + 1] = 0x0;
	}
	if (fbAddr == MAP_FAILED) {
		::close(fd);
		mDebug("unable to map fb memory, error is %d", errno);
		return -EPERM;
	}
	return 0;
}

FbOutput::FbOutput(QObject *parent) :
	BaseLmmOutput(parent)
{
	fd = -1;
}

int FbOutput::outputBuffer(RawBuffer *buf)
{
	qint64 time = streamTime->getCurrentTime() - streamTime->getStartTime();
	const char *data = (const char *)buf->constData();
	if (fd > 0) {
		if (buf->size() == fbSize)
			memcpy(fbAddr, data, fbSize);
		else {
			int inW = buf->getBufferParameter("width").toInt();
			int inH = buf->getBufferParameter("height").toInt();
			int inSize = inH * inW * 2;
			int startX = fbLineLen / 2 - inW;
			if (startX < 0)
				startX = 0;
			int startY = (fbSize / fbLineLen - inH) / 2 * fbLineLen;
			if (startY < 0)
				startY = 0;
			mInfo("buffer=%d time=%lld frame: %d x %d, fbsize is %d, ts is %lld",
				  buf->streamBufferNo(), time / 1000, inW, inH, fbSize,
				  (buf->getPts()) / 1000);
			int j = 0;
			for (int i = startY; i < fbSize; i += fbLineLen) {
				memcpy(fbAddr + i + startX, data + j, inW * 2);
				j += inW * 2;
				if (j >= inSize)
					break;
			}
		}
	} else
		mDebug("fb device is not opened");

	return 0;
}

int FbOutput::start()
{
	int err = openFb("/dev/fb3");
	if (err) {
		mDebug("error opening framebuffer");
		return err;
	}

	return BaseLmmOutput::start();
}

int FbOutput::stop()
{
	if (fd == -1)
		return 0;
	munmap(fbAddr, fbSize);
	::close(fd);
	return BaseLmmOutput::stop();
}

int FbOutput::flush()
{
	return BaseLmmOutput::flush();
}
