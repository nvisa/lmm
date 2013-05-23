#include "dmaiencoder.h"
#include "dmai/dmaibuffer.h"
#include "streamtime.h"
#include "tools/unittimestat.h"

#include "debug.h"

#include <ti/sdo/ce/CERuntime.h>

#include <errno.h>

#include <QTime>
#include <QSemaphore>

DmaiEncoder::DmaiEncoder(QObject *parent) :
	BaseLmmElement(parent)
{
	maxFrameRate = 30000;
	rateControl = RATE_NONE;
	videoBitRate = -1;
	intraFrameInterval = 30;
	imageWidth = 1280;
	imageHeight = 720;
	codec = CODEC_H264;
	encodeTimeStat = new UnitTimeStat;
	encodeTiming = new QTime;
}

int DmaiEncoder::setCodecType(DmaiEncoder::CodecType type)
{
	if (type == CODEC_MPEG2)
		return -EINVAL;
	codec = type;
	return 0;
}

void DmaiEncoder::setFrameRate(float fps)
{
	frameRate = fps;
	maxFrameRate = frameRate * 1000;
}

float DmaiEncoder::getFrameRate()
{
	return frameRate;
}

int DmaiEncoder::start()
{
	int w = getParameter("videoWidth").toInt();
	int h = getParameter("videoHeight").toInt();
	if (w)
		imageWidth = w;
	if (h)
		imageHeight = h;
	encodeTimeStat->reset();
	encodeTiming->start();
	encodeCount = 0;
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
	if (hEngine) {
		mDebug("closing codec engine");
		Engine_close(hEngine);
		hEngine = NULL;
	}
	return BaseLmmElement::stop();
}

int DmaiEncoder::flush()
{
	if (codec == CODEC_H264 && encodeCount) {
		mDebug("flusing encoder, will generate IDR frame");
		generateIdrFrame = true;
		if (dirty) {
			stopCodec();
			startCodec();
			dirty = false;
		}
	}
	return BaseLmmElement::flush();
}

int DmaiEncoder::encodeNext()
{
	QTime t;
	int err = 0;
	inputLock.lock();
	if (inputBuffers.size() == 0) {
		inputLock.unlock();
		return -ENOENT;
	}
	RawBuffer buf = inputBuffers.first();
	inputLock.unlock();
	Buffer_Handle dmai = (Buffer_Handle)buf.getBufferParameter("dmaiBuffer")
			.toInt();
	if (!dmai) {
		mDebug("cannot get dmai buffer");
		err = -ENOENT;
		goto out;
	}
	t.start();
	Buffer_setNumBytesUsed(dmai, buf.size());
	err = encode(dmai, buf);
	mInfo("encode took %d msecs", t.elapsed());
	encodeTimeStat->addStat(encodeTiming->restart());
	if (encodeTimeStat->last > 75)
		mInfo("late encode: %d", encodeTimeStat->last);
	if (err)
		goto out;
	inputLock.lock();
	if (!inputBuffers.isEmpty())
		inputBuffers.removeFirst();
	inputLock.unlock();
	releaseOutputSem(0);
	return 0;
out:
	return err;
}

int DmaiEncoder::encodeNextBlocking()
{
	if (!acquireInputSem(0))
		return -EINVAL;
	return encodeNext();
}

void DmaiEncoder::aboutDeleteBuffer(const QMap<QString, QVariant> &params)
{
	Buffer_Handle dmai = (Buffer_Handle)params["dmaiBuffer"].toInt();
	bufferLock.lock();
	BufTab_freeBuf(dmai);
	bufferLock.unlock();
	mInfo("freeing buffer");
}

void DmaiEncoder::initCodecEngine()
{
	CERuntime_init();
	Dmai_init();
}

int DmaiEncoder::startCodec()
{
	IVIDENC1_DynamicParams defaultDynParams = Venc1_DynamicParams_DEFAULT;
	VIDENC1_Params          defaultParams       = Venc1_Params_DEFAULT;
	BufferGfx_Attrs         gfxAttrs            = BufferGfx_Attrs_DEFAULT;
	VIDENC1_Params         *params;

	/* DM365 only supports YUV420P semi planar chroma format */
	ColorSpace_Type         colorSpace = ColorSpace_YUV420PSEMI;
	Bool                    localBufferAlloc = TRUE;
	Int32                   bufSize;
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
	dynParams = new IVIDENC1_DynamicParams;
	*dynParams = defaultDynParams;

	/*
	 * Set up codec parameters. We round up the height to accomodate for
	 * alignment restrictions from codecs
	 */
	params->maxWidth = imageWidth;
	params->maxHeight = Dmai_roundUp(imageHeight, CODECHEIGHTALIGN);
	/*
		 * According to codec user guide, high speed or high quality
		 * setting does not make any difference on bitstream/performance
		 */
	params->encodingPreset  = XDM_HIGH_SPEED;
	if (colorSpace ==  ColorSpace_YUV420PSEMI) {
		params->inputChromaFormat = XDM_YUV_420SP;
	} else {
		params->inputChromaFormat = XDM_YUV_422ILE;
	}
	params->reconChromaFormat = XDM_YUV_420SP;
	params->maxFrameRate      = maxFrameRate;

	if (rateControl == RATE_CBR) {
		params->rateControlPreset = IVIDEO_LOW_DELAY;
		params->maxBitRate = videoBitRate;
	} else if (rateControl == RATE_VBR) {
		params->rateControlPreset = IVIDEO_STORAGE;
		params->maxBitRate = 50000000;
	} else {
		params->rateControlPreset = IVIDEO_NONE;
		params->maxBitRate = 50000000; //bogus, should be > 0
	}

	dynParams->targetBitRate   = params->maxBitRate;
	dynParams->inputWidth      = imageWidth;
	dynParams->inputHeight     = imageHeight;
	dynParams->refFrameRate    = params->maxFrameRate;
	dynParams->targetFrameRate = params->maxFrameRate;
	dynParams->interFrameInterval = 0;
	dynParams->intraFrameInterval = intraFrameInterval;

	QString codecName;
	if (codec == CODEC_H264)
		codecName = "h264enc";
	else if (codec == CODEC_MPEG4)
		codecName = "mpeg4enc";
	else
		return -EINVAL;
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
		hBufTab = BufTab_create(4, bufSize,
								BufferGfx_getBufferAttrs(&gfxAttrs));

		if (hBufTab == NULL) {
			mDebug("Failed to allocate contiguous buffers");
			return -ENOMEM;
		}

	}

	return 0;
}


int DmaiEncoder::encode(Buffer_Handle buffer, const RawBuffer source)
{
	bool idrGenerated = false;
	mInfo("start");
	bufferLock.lock();
	Buffer_Handle hDstBuf = BufTab_getFreeBuf(hBufTab);
	bufferLock.unlock();
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

	if (generateIdrFrame) {
		mDebug("generating IDR");
		VIDENC1_Status status;
		status.size = sizeof(VIDENC1_Status);
		dynParams->forceFrame = IVIDEO_IDR_FRAME;
		if (VIDENC1_control(Venc1_getVisaHandle(hCodec), XDM_SETPARAMS,
							dynParams, &status) != VIDENC1_EOK) {
			qDebug("error setting control on encoder: 0x%x", (int)status.extendedError);
		} else
			idrGenerated = true;
	}

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
		bufferLock.lock();
		BufTab_freeBuf(hDstBuf);
		bufferLock.unlock();
		return -EIO;
	}

	if (idrGenerated) {
		VIDENC1_Status status;
		status.size = sizeof(VIDENC1_Status);
		dynParams->forceFrame = IVIDEO_NA_FRAME;
		if (VIDENC1_control(Venc1_getVisaHandle(hCodec), XDM_SETPARAMS,
							dynParams, &status) != VIDENC1_EOK)
			qDebug("error setting control on encoder: 0x%x", (int)status.extendedError);
		generateIdrFrame = false;
	}

	RawBuffer buf = DmaiBuffer("video/x-h264", hDstBuf, this);
	int frameType = BufferGfx_getFrameType(buffer);
	if (frameType == IVIDEO_IDR_FRAME)
		qDebug("IDR frame generated");
	buf.addBufferParameters(source.bufferParameters());
	buf.addBufferParameter("frameType", frameType);
	buf.addBufferParameter("encodeTime", streamTime->getCurrentTime());
	buf.setStreamBufferNo(encodeCount++);
	buf.setDuration(1000 / buf.getBufferParameter("fps").toFloat());
	Buffer_setUseMask(hDstBuf, Buffer_getUseMask(hDstBuf) | 0x1);
	/* Reset the dimensions to what they were originally */
	BufferGfx_resetDimensions(buffer);
	outputLock.lock();
	outputBuffers << buf;
	outputLock.unlock();

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

	return 0;
}
