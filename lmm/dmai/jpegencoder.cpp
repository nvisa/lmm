#include "jpegencoder.h"
#include "dmai/dmaibuffer.h"
#include "streamtime.h"

#include "debug.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/ce/Ienc1.h>

#include <errno.h>

#define CODECHEIGHTALIGN 16

JpegEncoder::JpegEncoder(QObject *parent) :
	DmaiEncoder(parent)
{
	codec = CODEC_JPEG;
	qFact = 97;
	qFactChanged = false;
	bufferCount = 0;
	maxBufferSize = 0;
}

void JpegEncoder::setQualityFactor(int q)
{
	if (q == qFact)
		return;
	if (q > 100)
		q = 100;
	if (q < 3)
		q = 3;
	qFact = q;
	dspl.lock();
	qFactChanged = true;
	dspl.unlock();
}

int JpegEncoder::qualityFactor()
{
	return qFact;
}

void JpegEncoder::setBufferCount(int cnt, int maxSize)
{
	bufferCount = cnt;
	maxBufferSize = maxSize;
}

int JpegEncoder::startCodec(bool alloc)
{
	defaultDynParams = Ienc1_DynamicParams_DEFAULT;
	IMGENC1_Params          defaultParams       = Ienc1_Params_DEFAULT;
	BufferGfx_Attrs         gfxAttrs            = BufferGfx_Attrs_DEFAULT;
	IMGENC1_Params         *params;
	IMGENC1_DynamicParams  *dynParams;

	/* DM365 only supports YUV420P semi planar chroma format */
	ColorSpace_Type         colorSpace = ColorSpace_YUV420PSEMI;
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

	if (colorSpace ==  ColorSpace_YUV420PSEMI) {
		dynParams->inputChromaFormat = XDM_YUV_420SP;
	} else {
		dynParams->inputChromaFormat = XDM_YUV_422ILE;
	}
	dynParams->inputWidth = imageWidth;
	dynParams->inputHeight = imageHeight;
	dynParams->captureWidth = imageWidth;
	dynParams->qValue = qFact;

	QString codecName = "jpegenc";
	/* Create the video encoder */
	hCodec = Ienc1_create(hEngine, (Char *)qPrintable(codecName), params, dynParams);
	if (hCodec == NULL) {
		mDebug("Failed to create image encoder %s.", qPrintable(codecName));
		return -EINVAL;
	}

	if (alloc) {
		/* Allocate output buffers */
		Buffer_Attrs bAttrs = Buffer_Attrs_DEFAULT;
		outBufSize = maxBufferSize;
		if (!outBufSize)
			outBufSize = Ienc1_getOutBufSize(hCodec);
		if (bufferCount)
			outputBufTab = BufTab_create(bufferCount, outBufSize, &bAttrs);
		else
			outputBufTab = BufTab_create(1, outBufSize, &bAttrs);
		if (outputBufTab == NULL) {
			mDebug("unable to allocate output buffer tab of size %d for %d buffers", outBufSize, bufferCount);
			return -ENOMEM;
		}
	}

	return 0;
}

int JpegEncoder::stopCodec()
{
	/* Shut down remaining items */
	if (hCodec) {
		mDebug("closing jpeg encoder");
		Ienc1_delete(hCodec);
		hCodec = NULL;
	}

	return 0;
}

int JpegEncoder::encode(Buffer_Handle buffer, const RawBuffer source)
{
	mInfo("start");
	bufferLock.lock();
	Buffer_Handle hDstBuf = BufTab_getFreeBuf(outputBufTab);
	if (!hDstBuf) {
		/*
		 * This is not an error, probably buffers are not finished yet
		 * and we don't need to encode any more
		 */
		mInfo("cannot get new buf from buftab");
		bufferLock.unlock();
		return 0;
	}
	Buffer_setUseMask(hDstBuf, Buffer_getUseMask(hDstBuf) | 0x1);
	bufferLock.unlock();

	BufferGfx_Dimensions dim;
	/* Make sure the whole buffer is used for input */
	BufferGfx_resetDimensions(buffer);

	/* Ensure that the video buffer has dimensions accepted by codec */
	BufferGfx_getDimensions(buffer, &dim);
	dim.height = Dmai_roundUp(dim.height, CODECHEIGHTALIGN);
	BufferGfx_setDimensions(buffer, &dim);

	if (qFactChanged) {
		mDebug("changing quality to %d", qFact);
		IMGENC1_Status status;
		status.size = sizeof(IMGENC1_Status);
		IMGENC1_DynamicParams *dynParams = &defaultDynParams;
		dynParams->qValue = qFact;
		if (IMGENC1_control(Ienc1_getVisaHandle(hCodec), XDM_SETPARAMS,
							dynParams, &status) != IMGENC1_EOK) {
			mDebug("error setting control on encoder: 0x%x", (int)status.extendedError);
		}
		qFactChanged = false;
	}

	mInfo("invoking Ienc1_process: width=%d height=%d",
		  (int)dim.width, (int)dim.height);
	/* Encode the video buffer */
	if (Ienc1_process(hCodec, buffer, hDstBuf) < 0) {
		mDebug("Failed to encode video buffer");
		BufferGfx_getDimensions(buffer, &dim);
		mInfo("colorspace=%d dims: x=%d y=%d width=%d height=%d linelen=%d",
			  BufferGfx_getColorSpace(buffer),
			  (int)dim.x, (int)dim.y, (int)dim.width,
			  (int)dim.height, (int)dim.lineLength);
		bufferLock.lock();
		BufTab_freeBuf(hDstBuf);
		bufferLock.unlock();
		return -EIO;
	}
	RawBuffer buf;
	if (bufferCount)
		buf = DmaiBuffer("image/jpeg", hDstBuf, this);
	else
		buf = RawBuffer("image/jpeg", Buffer_getUserPtr(hDstBuf), Buffer_getNumBytesUsed(hDstBuf));
	buf.setParameters(source.constPars());
	buf.pars()->frameType = IVIDEO_I_FRAME;
	buf.pars()->encodeTime = streamTime->getCurrentTime();
	buf.pars()->streamBufferNo = source.constPars()->streamBufferNo;
	if (bufferCount)
		buf.pars()->dmaiBuffer = (quintptr *)hDstBuf;
	if (buf.pars()->fps)
		buf.pars()->duration = 1000 / buf.pars()->fps;
	/* Reset the dimensions to what they were originally */
	BufferGfx_resetDimensions(buffer);
	newOutputBuffer(0, buf);
	if (!bufferCount) {
		bufferLock.lock();
		BufTab_freeBuf(hDstBuf);
		bufferLock.unlock();
	}

	return 0;
}
