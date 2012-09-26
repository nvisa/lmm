#include "dm365videooutput.h"
#include "rawbuffer.h"
#include "dmai/dmaibuffer.h"
#include "tools/videoutils.h"

#include <emdesk/debug.h>

#include <errno.h>

#include <linux/videodev2.h>

#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Display.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/ColorSpace.h>
#include <ti/sdo/dmai/Framecopy.h>

static BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
static Display_Attrs dAttrs = Display_Attrs_DM365_VID_DEFAULT;

void DM365VideoOutput::
	videoCopy(RawBuffer buf, Buffer_Handle dispbuf, Buffer_Handle dmai)
{
	if (hFrameCopy == NULL) {
		int linelen = VideoUtils::getLineLength(buf.getBufferParameter("v4l2PixelFormat").toInt(),
												buf.getBufferParameter("width").toInt());
		char *dst = (char *)Buffer_getUserPtr(dispbuf);
		char *src = (char *)Buffer_getUserPtr(dmai);
		int dstTotal = gfxAttrs.dim.lineLength * gfxAttrs.dim.height;
		int j = 0;
		for (int i = 0; i < dstTotal; i += gfxAttrs.dim.lineLength) {
			memcpy(dst + i, src + j, gfxAttrs.dim.lineLength);
			j += linelen;
		}
	} else {
		if (!frameCopyConfigured) {
			Framecopy_config(hFrameCopy, dmai, dispbuf);
			frameCopyConfigured = true;
		}
		Framecopy_execute(hFrameCopy, dmai, dispbuf);
	}
}

DM365VideoOutput::DM365VideoOutput(QObject *parent) :
	V4l2Output(parent)
{
	outputType = Lmm::COMPONENT;
	pixelFormat = V4L2_PIX_FMT_NV12;

	Framecopy_Attrs fcAttrs;
	fcAttrs.accel = true;
	fcAttrs.rszRate = 0;
	fcAttrs.sdma = false;
	hFrameCopy = Framecopy_create(&fcAttrs);
	if (hFrameCopy)
		frameCopyConfigured = false;
}

void DM365VideoOutput::setVideoOutput(Lmm::VideoOutput out)
{
	if (outputType == out)
		return;
	 outputType = out;
	 if (getState() == STARTED) {
		 /* we already adjusted output, need to re-adjust */
		 inputLock.lock();
		 outputLock.lock();
		 inputBuffers.clear();
		 outputBuffers.clear();
		 stop();
		 start();
		 outputLock.unlock();
		 inputLock.unlock();
	 }
}

int DM365VideoOutput::outputBuffer(RawBuffer buf)
{
	if (outputType == Lmm::COMPONENT)
		return V4l2Output::outputBuffer(buf);
	/* input size does not match output size, do manual copy */
	Buffer_Handle dispbuf;
	Display_get(hDisplay, &dispbuf);
	Buffer_Handle dmai = (Buffer_Handle)buf.getBufferParameter("dmaiBuffer")
			.toInt();
	videoCopy(buf, dispbuf, dmai);
	Display_put(hDisplay, dispbuf);

	return 0;
}

int DM365VideoOutput::start()
{
	int bufSize;
	ColorSpace_Type colorSpace = ColorSpace_YUV420PSEMI;
	if (pixelFormat == V4L2_PIX_FMT_UYVY)
		colorSpace = ColorSpace_UYVY;
	if (outputType == Lmm::COMPONENT) {
		gfxAttrs.dim.width = 1280;
		gfxAttrs.dim.height = 720;
	} else if (outputType == Lmm::COMPOSITE) {
		gfxAttrs.dim.width = 720;
		gfxAttrs.dim.height = 576;
	} else
		return -EINVAL;
	gfxAttrs.dim.lineLength =
			Dmai_roundUp(BufferGfx_calcLineLength(gfxAttrs.dim.width,
												  colorSpace), 32);
	gfxAttrs.dim.x = 0;
	gfxAttrs.dim.y = 0;
	if (colorSpace ==  ColorSpace_YUV420PSEMI) {
		bufSize = gfxAttrs.dim.lineLength * gfxAttrs.dim.height * 3 / 2;
	}
	else {
		bufSize = gfxAttrs.dim.lineLength * gfxAttrs.dim.height * 2;
	}
	/* Create a table of buffers to use with the capture driver */
	gfxAttrs.colorSpace = colorSpace;
	hDispBufTab = BufTab_create(3, bufSize,
								BufferGfx_getBufferAttrs(&gfxAttrs));
	if (hDispBufTab == NULL) {
		mDebug("Failed to create buftab");
		return -ENOMEM;
	}
	if (outputType == Lmm::COMPONENT) {
		dAttrs.videoStd = VideoStd_720P_60;
		dAttrs.videoOutput = Display_Output_COMPONENT;
		dAttrs.width = gfxAttrs.dim.width;
		dAttrs.height = gfxAttrs.dim.height;
	} else if (outputType == Lmm::COMPOSITE) {
		dAttrs.videoStd = VideoStd_D1_PAL;
		dAttrs.videoOutput = Display_Output_COMPOSITE;
		dAttrs.width = gfxAttrs.dim.width;
		dAttrs.height = gfxAttrs.dim.height;
	} else
		return -EINVAL;
	dAttrs.numBufs = 3;
	dAttrs.colorSpace = colorSpace;
	hDisplay = Display_create(hDispBufTab, &dAttrs);
	if (hDisplay == NULL) {
		mDebug("Failed to create display device");
		return -EINVAL;
	}
	for (int i = 0; i < BufTab_getNumBufs(hDispBufTab); i++) {
		Buffer_Handle dmaibuf = BufTab_getBuf(hDispBufTab, i);
		DmaiBuffer newbuf = DmaiBuffer("video/x-raw-yuv", dmaibuf, this);
		newbuf.addBufferParameter("v4l2PixelFormat", (int)pixelFormat);
		bufferPool.insert(dmaibuf, newbuf);
	}

	return BaseLmmOutput::start();
}

int DM365VideoOutput::stop()
{
	return V4l2Output::stop();
}
