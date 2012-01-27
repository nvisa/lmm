#include "dmaiencoder.h"
#include "emdesk/debug.h"

#include <errno.h>

#define VIDEO_PIPE_SIZE 4
#define CODECHEIGHTALIGN 16

DmaiEncoder::DmaiEncoder(QObject *parent) :
	BaseLmmElement(parent)
{
	imageWidth = 720;
	imageHeight = 576;
}

void DmaiEncoder::setImageSize(QSize s)
{
	imageWidth = s.width();
	imageHeight = s.height();
}

int DmaiEncoder::start()
{
	int err = startCodec();
	if (err)
		return err;
	return BaseLmmElement::start();
}

int DmaiEncoder::stop()
{
	int err = stopCodec();
	if (err)
		return err;
	return BaseLmmElement::stop();
}

int DmaiEncoder::encode(Buffer_Handle buffer)
{
	BufferGfx_Dimensions dim;
	/* Make sure the whole buffer is used for input */
	BufferGfx_resetDimensions(buffer);

	/* Ensure that the video buffer has dimensions accepted by codec */
	BufferGfx_getDimensions(buffer, &dim);
	dim.height = Dmai_roundUp(dim.height, CODECHEIGHTALIGN);
	BufferGfx_setDimensions(buffer, &dim);

	/* Encode the video buffer */
	if (Venc1_process(hCodec, buffer, hDstBuf) < 0) {
		mDebug("Failed to encode video buffer");
		return -EIO;
	}

	/* Reset the dimensions to what they were originally */
	BufferGfx_resetDimensions(buffer);
	return 0;
}

int DmaiEncoder::startCodec()
{
	VIDENC1_Params          defaultParams       = Venc1_Params_DEFAULT;
	VIDENC1_DynamicParams   defaultDynParams    = Venc1_DynamicParams_DEFAULT;
	BufferGfx_Attrs         gfxAttrs            = BufferGfx_Attrs_DEFAULT;
	VIDENC1_Params         *params;
	VIDENC1_DynamicParams  *dynParams;
	ColorSpace_Type         colorSpace = ColorSpace_YUV420PSEMI;
	Bool                    localBufferAlloc = TRUE;
	Int32                   bufSize;
	int videoBitRate = -1;
	int outBufSize;

	const char *engineName = "codecServer";
	/* Open the codec engine */
	hEngine = Engine_open((char *)engineName, NULL, NULL);

	if (hEngine == NULL) {
		mDebug("Failed to open codec engine %s", engineName);
		return -ENOENT;
	}
#if 0
	/* TODO: In case of HD resolutions the video buffer will be allocated
		   by capture thread. */
	if (((envp->imageWidth == VideoStd_720P_WIDTH) &&
		 (envp->imageHeight == VideoStd_720P_HEIGHT)) ||
			((envp->imageWidth == VideoStd_1080I_WIDTH) &&
			 (envp->imageHeight == VideoStd_1080I_HEIGHT))) {
		localBufferAlloc = FALSE;
	}
#endif

	/* Use supplied params if any, otherwise use defaults */
	params = &defaultParams;
	dynParams = &defaultDynParams;

	/*
	 * Set up codec parameters. We round up the height to accomodate for
	 * alignment restrictions from codecs
	 */
	params->maxWidth = imageWidth;
	params->maxHeight = Dmai_roundUp(imageHeight, CODECHEIGHTALIGN);
	params->encodingPreset  = XDM_HIGH_SPEED;
	if (colorSpace ==  ColorSpace_YUV420PSEMI) {
		params->inputChromaFormat = XDM_YUV_420SP;
	} else {
		params->inputChromaFormat = XDM_YUV_422ILE;
	}
	params->reconChromaFormat = XDM_YUV_420SP;
	params->maxFrameRate      = 25000;

	/* Set up codec parameters depending on bit rate */
	if (videoBitRate < 0) {
		/* Variable bit rate */
		params->rateControlPreset = IVIDEO_NONE;
		/*
		 * If variable bit rate use a bogus bit rate value (> 0)
		 * since it will be ignored.
		 */
		params->maxBitRate        = 2000000;
	} else {
		/* Constant bit rate */
		params->rateControlPreset = IVIDEO_STORAGE;
		params->maxBitRate        = videoBitRate;
	}

	dynParams->targetBitRate   = params->maxBitRate;
	dynParams->inputWidth      = imageWidth;
	dynParams->inputHeight     = imageHeight;
	dynParams->refFrameRate    = params->maxFrameRate;
	dynParams->targetFrameRate = params->maxFrameRate;
	dynParams->interFrameInterval = 0;

	QString codecName = "mpeg4enc";
	/* Create the video encoder */
	hCodec = Venc1_create(hEngine, (Char *)qPrintable(codecName), params, dynParams);

	if (hCodec == NULL) {
		mDebug("Failed to create video encoder: %s", qPrintable(codecName));
		return -EINVAL;
	}

	/* Store the output buffer size in the environment */
	outBufSize = Venc1_getOutBufSize(hCodec);

	if (localBufferAlloc == TRUE) {
		gfxAttrs.colorSpace = colorSpace;
		gfxAttrs.dim.width  = imageWidth;
		gfxAttrs.dim.height = imageHeight;
		gfxAttrs.dim.lineLength =
				Dmai_roundUp(BufferGfx_calcLineLength(gfxAttrs.dim.width,
													  gfxAttrs.colorSpace), 32);
		/*
		 * Calculate size of codec buffers based on rounded LineLength
		 * This will allow to share the buffers with Capture thread. If the
		 * size of the buffers was obtained from the codec through
		 * Venc1_getInBufSize() there would be a mismatch with the size of
		 * Capture thread buffers when the width is not 32 byte aligned.
		 */
		if (colorSpace ==  ColorSpace_YUV420PSEMI) {
			bufSize = gfxAttrs.dim.lineLength * gfxAttrs.dim.height * 3 / 2;
		} else {
			bufSize = gfxAttrs.dim.lineLength * gfxAttrs.dim.height * 2;
		}

		/* Create a table of buffers with size based on rounded LineLength */
		hBufTab = BufTab_create(VIDEO_PIPE_SIZE, bufSize,
								BufferGfx_getBufferAttrs(&gfxAttrs));

		if (hBufTab == NULL) {
			mDebug("Failed to allocate contiguous buffers");
			return -ENOMEM;
		}
#if 0
		/* Send buffers to the capture thread to be ready for main loop */
		for (bufIdx = 0; bufIdx < VIDEO_PIPE_SIZE; bufIdx++) {
			if (Fifo_put(envp->hCaptureInFifo,
						 BufTab_getBuf(hBufTab, bufIdx)) < 0) {
				ERR("Failed to send buffer to display thread\n");
				cleanup(THREAD_FAILURE);
			}
		}
#else
		/* TODO: This set by me */
		Venc1_setBufTab(hCodec, hBufTab);
#endif
	} else {
#if 0
		/* Send buffers to the capture thread to be ready for main loop */
		for (bufIdx = 0; bufIdx < VIDEO_PIPE_SIZE; bufIdx++) {
			fifoRet = Fifo_get(envp->hCaptureOutFifo, &hCapBuf);

			if (fifoRet < 0) {
				ERR("Failed to get buffer from capture thread\n");
				cleanup(THREAD_FAILURE);
			}

			/* Did the capture thread flush the fifo? */
			if (fifoRet == Dmai_EFLUSH) {
				cleanup(THREAD_SUCCESS);
			}
			/* Return buffer to capture thread */
			if (Fifo_put(envp->hCaptureInFifo, hCapBuf) < 0) {
				ERR("Failed to send buffer to display thread\n");
				cleanup(THREAD_FAILURE);
			}
		}
#endif
	}

	return 0;
}

int DmaiEncoder::stopCodec()
{
	/* Shut down remaining items */
	if (hCodec) {
		mDebug("closing video encoder");
		Venc1_delete(hCodec);
		hCodec = NULL;
	}

	if (hEngine) {
		mDebug("closing codec engine");
		Engine_close(hEngine);
		hEngine = NULL;
	}

	return 0;
}

