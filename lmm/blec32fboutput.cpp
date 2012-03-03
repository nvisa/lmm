#include "blec32fboutput.h"
#include "rawbuffer.h"
#include "dmaidecoder.h"

#include <emdesk/debug.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Resize.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>

Blec32FbOutput::Blec32FbOutput(QObject *parent) :
	FbOutput(parent)
{
	hResize = NULL;
}

int Blec32FbOutput::outputBuffer(RawBuffer *buf)
{
	Buffer_Handle dmaiBuf = (Buffer_Handle)buf->getBufferParameter("dmaiBuffer").toInt();
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
			return FbOutput::outputBuffer(buf);
		}
	} else
		mDebug("fb device is not opened");

	if (dmaiBuf)
		Buffer_freeUseMask(dmaiBuf, DmaiDecoder::OUTPUT_USE);
	return 0;
}

int Blec32FbOutput::start()
{
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
	return FbOutput::start();
}

int Blec32FbOutput::stop()
{
	if (hResize) {
		Resize_delete(hResize);
		Buffer_delete(fbOutBuf);
		hResize = NULL;
	}
	return FbOutput::stop();
}

int Blec32FbOutput::flush()
{
	foreach (RawBuffer *buf, inputBuffers) {
		Buffer_Handle dmaiBuf = (Buffer_Handle)buf->getBufferParameter("dmaiBuffer").toInt();
		Buffer_freeUseMask(dmaiBuf, DmaiDecoder::OUTPUT_USE);
	}
	return FbOutput::flush();
}