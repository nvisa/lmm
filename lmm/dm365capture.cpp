#include "dm365capture.h"
#include "emdesk/debug.h"

#include <errno.h>

#define NUM_CAPTURE_BUFS 3

DM365Capture::DM365Capture(QObject *parent) :
	BaseLmmElement(parent)
{
	hCapture = NULL;
	hBufTab = NULL;
}

QSize DM365Capture::captureSize()
{
	return QSize(imageWidth, imageHeight);
}

int DM365Capture::putFrame(Buffer_Handle handle)
{
	if (Capture_put(hCapture, handle) < 0) {
		mDebug("Failed to put capture buffer");
		return -EINVAL;
	}
	return 0;
}

Buffer_Handle DM365Capture::getFrame()
{
	Buffer_Handle handle;
	if (Capture_get(hCapture, &handle) < 0) {
		mDebug("Failed to get capture buffer");
		return NULL;
	}
	return handle;
}

int DM365Capture::openCamera()
{
	Capture_Attrs cAttrs   = Capture_Attrs_DM365_DEFAULT;
	BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
	BufferGfx_Dimensions  capDim;
	VideoStd_Type         videoStd;
	Int32                 bufSize;
	ColorSpace_Type       colorSpace = ColorSpace_YUV420PSEMI;
	Int                   numCapBufs;

	/* Create capture device driver instance */
	cAttrs.numBufs = NUM_CAPTURE_BUFS;
	cAttrs.videoInput = Capture_Input_COMPOSITE;
	cAttrs.videoStd = VideoStd_D1_PAL;
	cAttrs.colorSpace = colorSpace;

	videoStd = VideoStd_D1_PAL;

	/* Note: we only support D1, 720P and 1080P input */

	/* Calculate the dimensions of a video standard given a color space */
	if (BufferGfx_calcDimensions(videoStd, colorSpace, &capDim) < 0) {
		mDebug("Failed to calculate Buffer dimensions");
		return -EINVAL;
	}

	/*
	 * Capture driver provides 32 byte aligned data. We 32 byte align the
	 * capture and video buffers to perform zero copy encoding.
	 */
	capDim.width = Dmai_roundUp(capDim.width,32);
	imageWidth = capDim.width;
	imageHeight = capDim.height;

	numCapBufs = NUM_CAPTURE_BUFS;

	gfxAttrs.dim.height = capDim.height;
	gfxAttrs.dim.width = capDim.width;
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
	hBufTab = BufTab_create(numCapBufs, bufSize,
							BufferGfx_getBufferAttrs(&gfxAttrs));
	if (hBufTab == NULL) {
		mDebug("Failed to create buftab");
		return -ENOMEM;
	}

#if 0
	/* Create a table of buffers to use to prime Fifo to video thread */
	hFifoBufTab = BufTab_create(VIDEO_PIPE_SIZE, bufSize,
								BufferGfx_getBufferAttrs(&gfxAttrs));
	if (hFifoBufTab == NULL) {
		mDebug("Failed to create buftab\n");
		return -ENOMEM;
	}
#endif

	cAttrs.numBufs    = NUM_CAPTURE_BUFS;
	cAttrs.colorSpace = colorSpace;
	cAttrs.captureDimension = &gfxAttrs.dim;
	/* Create the capture device driver instance */
	hCapture = Capture_create(hBufTab, &cAttrs);

	if (hCapture == NULL) {
		mDebug("Failed to create capture device. Is video input connected?");
		return -ENOENT;
	}

	return 0;
}

int DM365Capture::closeCamera()
{
	if (hCapture) {
		mDebug("deleting capture handle");
		Capture_delete(hCapture);
		hCapture = NULL;
	}

	/* Clean up the thread before exiting */
	if (hBufTab) {
		mDebug("deleting capture buffer tab");
		BufTab_delete(hBufTab);
		hBufTab = NULL;
	}
	return 0;
}
