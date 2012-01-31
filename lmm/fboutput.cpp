#include "fboutput.h"
#define DEBUG
#include "emdesk/debug.h"
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

#include <ti/sdo/dmai/Dmai.h>
/*#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Vdec2.h>
#include <ti/sdo/dmai/Time.h>*/
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>

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
	BaseLmmOutput(parent)
{
	fd = -1;
	hResize = NULL;
}

int FbOutput::outputBuffer(RawBuffer *buf)
{
	qint64 time = streamTime->getCurrentTime() - streamTime->getStartTime();
	Buffer_Handle dmaiBuf = (Buffer_Handle)buf->getBufferParameter("dmaiBuffer").toInt();
	const char *data = (const char *)buf->constData();
	if (fd > 0) {
		if (hResize) {
			if (!resizerConfigured) {
				if (Resize_config(hResize, dmaiBuf, fbOutBuf) < 0) {
					mDebug("Failed to configure resizer");
				}
				resizerConfigured = true;
			}

			if (Resize_execute(hResize, dmaiBuf, fbOutBuf) < 0) {
				mDebug("Failed to execute resizer");
			}
		} else {
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
					   buf->streamBufferNo(), time / 1000, inW, inH, fbSize, (buf->getPts()) / 1000);
				int j = 0;
				for (int i = startY; i < fbSize; i += fbLineLen) {
					memcpy(fbAddr + i + startX, data + j, inW * 2);
					j += inW * 2;
					if (j >= inSize)
						break;
				}
			}
		}
	} else
		mDebug("fb device is not opened");

	Buffer_freeUseMask(dmaiBuf, DmaiDecoder::OUTPUT_USE);
	return 0;
}

int FbOutput::start()
{
	int err = openFb("/dev/fb3");
	if (err)
		mDebug("error opening framebuffer");

	Resize_Attrs rAttrs = Resize_Attrs_DEFAULT;
	hResize = Resize_create(&rAttrs);
	BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
	gfxAttrs.colorSpace = ColorSpace_UYVY;
	gfxAttrs.dim.width = fbLineLen / 2;
	gfxAttrs.dim.height = fbHeight;
	gfxAttrs.dim.lineLength = fbLineLen;
	gfxAttrs.bAttrs.useMask = 0;
	gfxAttrs.bAttrs.reference = TRUE;
	fbOutBuf = Buffer_create(0, BufferGfx_getBufferAttrs(&gfxAttrs));
	Buffer_setUserPtr(fbOutBuf, (Int8 *)fbAddr);
	Buffer_setNumBytesUsed(fbOutBuf, fbLineLen * fbHeight);
	resizerConfigured = false;

	return BaseLmmElement::start();
}

int FbOutput::stop()
{
	if (fd == -1)
		return 0;
	munmap(fbAddr, fbSize);
	::close(fd);
	if (hResize) {
		Resize_delete(hResize);
		Buffer_delete(fbOutBuf);
		hResize = NULL;
	}
	return BaseLmmElement::stop();
}

int FbOutput::flush()
{
	foreach (RawBuffer *buf, inputBuffers) {
		Buffer_Handle dmaiBuf = (Buffer_Handle)buf->getBufferParameter("dmaiBuffer").toInt();
		Buffer_freeUseMask(dmaiBuf, DmaiDecoder::OUTPUT_USE);
	}
	return BaseLmmOutput::flush();
}
