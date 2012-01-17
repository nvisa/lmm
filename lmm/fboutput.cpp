#include "fboutput.h"
#define DEBUG
#include "emdesk/debug.h"
#include "rawbuffer.h"

#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#include <QVariant>

#ifdef __cplusplus
extern "C" {
#endif
#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Vdec2.h>
#include <ti/sdo/dmai/Time.h>
#ifdef __cplusplus
}
#endif

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
		mDebug("unable to map fb memory");
		return -EPERM;
	}
	return 0;
}

FbOutput::FbOutput(QObject *parent) :
	QObject(parent)
{
	fd = -1;
	if (openFb("/dev/fb3"))
		mDebug("error opening framebuffer");
}

int FbOutput::addBuffer(RawBuffer *buffer)
{
	buffers << buffer;
	return 0;
}

int FbOutput::output()
{
	if (!buffers.size())
		return -ENOENT;
	RawBuffer *buf = buffers.takeFirst();
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
			mDebug("displaying video frame %d x %d, fbsize is %d", inW, inH, fbSize);
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
	Buffer_Handle dmaiBuf = (Buffer_Handle)buf->getBufferParameter("dmaiBuffer").toInt();
	BufTab_freeBuf(dmaiBuf);
	delete buf;
	return 0;
}
