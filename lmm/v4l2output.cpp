#include "v4l2output.h"
#include "dmai/dmaibuffer.h"

#include <emdesk/debug.h>

#include <errno.h>
#include <linux/videodev2.h>

#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Display.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/BufferGfx.h>

V4l2Output::V4l2Output(QObject *parent) :
	BaseLmmOutput(parent)
{
	dontDeleteBuffers = true;
}

int V4l2Output::outputBuffer(RawBuffer buf)
{
	Buffer_Handle dispbuf;
	Display_get(hDisplay, &dispbuf);
	outputBuffers << bufferPool[dispbuf];

	Buffer_Handle dmai = (Buffer_Handle)buf.getBufferParameter("dmaiBuffer")
			.toInt();
	Display_put(hDisplay, dmai);
	if (!bufferPool.contains(dmai))
		bufferPool.insert(dmai, buf);

	mDebug("displaying %p, returning %p", dispbuf, dmai);
	return 0;
}

int V4l2Output::start()
{
	BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
	Display_Attrs dAttrs = Display_Attrs_DM365_VID_DEFAULT;

	int bufSize;
	ColorSpace_Type colorSpace = ColorSpace_YUV420PSEMI;
	gfxAttrs.dim.width = 1280;
	gfxAttrs.dim.height = 720;
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
	dAttrs.videoStd = VideoStd_720P_60;
	dAttrs.videoOutput = Display_Output_COMPONENT;
	dAttrs.numBufs = 3;
	dAttrs.colorSpace = ColorSpace_YUV420PSEMI;
	dAttrs.width = 1280;
	dAttrs.height = 720;
	hDisplay = Display_create(hDispBufTab, &dAttrs);
	if (hDisplay == NULL) {
		mDebug("Failed to create display device");
		return -EINVAL;
	}
	for (int i = 0; i < BufTab_getNumBufs(hDispBufTab); i++) {
		Buffer_Handle dmaibuf = BufTab_getBuf(hDispBufTab, i);
		RawBuffer newbuf = DmaiBuffer("video/x-raw-yuv", dmaibuf, this);
		newbuf.addBufferParameter("v4l2PixelFormat", V4L2_PIX_FMT_NV12);
		bufferPool.insert(dmaibuf, newbuf);
	}

	return BaseLmmOutput::start();
}

int V4l2Output::stop()
{
	Display_delete(hDisplay);
	BufTab_delete(hDispBufTab);

	return BaseLmmOutput::stop();
}

void V4l2Output::aboutDeleteBuffer(const QMap<QString, QVariant> &params)
{
	//Buffer_Handle dmai = (Buffer_Handle)buf->getBufferParameter("dmaiBuffer")
		//	.toInt();
	//finishedDmaiBuffers << dmai;
	//mDebug("buffer finished");
}
