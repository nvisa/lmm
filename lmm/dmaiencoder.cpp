#include "dmaiencoder.h"
#include "rawbuffer.h"
#include "emdesk/debug.h"

#include <ti/sdo/ce/CERuntime.h>

#include <errno.h>

#define VIDEO_PIPE_SIZE 4
#define CODECHEIGHTALIGN 16

DmaiEncoder::DmaiEncoder(QObject *parent) :
	BaseLmmElement(parent)
{
	imageWidth = 1280;
	imageHeight = 720;
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

int DmaiEncoder::encodeNext()
{
	int err = 0;
	if (inputBuffers.size() == 0)
		return -ENOENT;
	RawBuffer *buf = inputBuffers.takeFirst();
	Buffer_Handle dmai = (Buffer_Handle)buf->getBufferParameter("dmaiBuffer")
			.toInt();
	if (!dmai) {
		mDebug("cannot get dmai buffer");
		err = -ENOENT;
		goto out;
	}
	err = encode(dmai);
	if (err)
		goto out;
out:
	delete buf;
	return err;
}

int DmaiEncoder::encode(Buffer_Handle buffer)
{
	mInfo("start");
	Buffer_Handle hDstBuf = BufTab_getFreeBuf(hBufTab);
	if (!hDstBuf) {
		/*
		 * This is not an error, probably buffers are not finished yet
		 * and we don't need to encode any more
		 */
		mInfo("cannot get new buf from buftab");
		return -ENOENT;
	}

	BufferGfx_Dimensions dim;
	/* Make sure the whole buffer is used for input */
	BufferGfx_resetDimensions(buffer);

	/* Ensure that the video buffer has dimensions accepted by codec */
	BufferGfx_getDimensions(buffer, &dim);
	dim.height = Dmai_roundUp(dim.height, CODECHEIGHTALIGN);
	BufferGfx_setDimensions(buffer, &dim);

	mInfo("invoking venc1_process: width=%d height=%d",
		  (int)dim.width, (int)dim.height);
	/* Encode the video buffer */
	if (Venc1_process(hCodec, buffer, hDstBuf) < 0) {
		mDebug("Failed to encode video buffer");
		BufferGfx_getDimensions(buffer, &dim);
		mInfo("colorspace=%d dims: x=%d y=%d width=%d height=%d linelen=%d",
			  BufferGfx_getColorSpace(buffer),
			  (int)dim.x, (int)dim.y, (int)dim.width,
			  (int)dim.height, (int)dim.lineLength);
		BufTab_freeBuf(hDstBuf);
		return -EIO;
	}
	RawBuffer *buf = new RawBuffer(this);
	buf->setRefData(Buffer_getUserPtr(hDstBuf), Buffer_getNumBytesUsed(hDstBuf));
	buf->addBufferParameter("dmaiBuffer", (int)hDstBuf);
	Buffer_setUseMask(hDstBuf, Buffer_getUseMask(hDstBuf) | 0x1);
	outputBuffers << buf;
	/* Reset the dimensions to what they were originally */
	BufferGfx_resetDimensions(buffer);

	return 0;
}

void DmaiEncoder::aboutDeleteBuffer(RawBuffer *buf)
{
	Buffer_Handle dmai = (Buffer_Handle)buf->getBufferParameter("dmaiBuffer")
			.toInt();
	BufTab_freeBuf(dmai);
}

void DmaiEncoder::initCodecEngine()
{
	CERuntime_init();
	Dmai_init();
}

int DmaiEncoder::startCodec()
{
	VIDENC1_Params          defaultParams       = Venc1_Params_DEFAULT;
	VIDENC1_DynamicParams   defaultDynParams    = Venc1_DynamicParams_DEFAULT;
	BufferGfx_Attrs         gfxAttrs            = BufferGfx_Attrs_DEFAULT;
	VIDENC1_Params         *params;
	VIDENC1_DynamicParams  *dynParams;
	/* DM365 only supports YUV420P semi planar chroma format */
	ColorSpace_Type         colorSpace = ColorSpace_YUV420PSEMI;
	Bool                    localBufferAlloc = TRUE;
	Int32                   bufSize;
	int videoBitRate = -1;
	int outBufSize;

	const char *engineName = "encode";
	/* Open the codec engine */
	hEngine = Engine_open((char *)engineName, NULL, NULL);

	if (hEngine == NULL) {
		mDebug("Failed to open codec engine %s", engineName);
		return -ENOENT;
	}

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

	QString codecName = "h264enc";
	/* Create the video encoder */
	hCodec = Venc1_create(hEngine, (Char *)qPrintable(codecName), params, dynParams);

	if (hCodec == NULL) {
		mDebug("Failed to create video encoder: %s", qPrintable(codecName));
		return -EINVAL;
	}

	/* Allocate output buffers */
	Buffer_Attrs bAttrs = Buffer_Attrs_DEFAULT;
	outBufSize = Venc1_getOutBufSize(hCodec);
	outputBufTab = BufTab_create(3, outBufSize, &bAttrs);
	if (outputBufTab == NULL) {
		mDebug("unable to allocate output buffer tab of size %d for %d buffers", outBufSize, 3);
		return -ENOMEM;
	}

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

	BufTab_delete(hBufTab);
	BufTab_delete(outputBufTab);

	if (hEngine) {
		mDebug("closing codec engine");
		Engine_close(hEngine);
		hEngine = NULL;
	}

	return 0;
}

