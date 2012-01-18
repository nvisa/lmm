#include <QThread>
#include <QVariant>

#include "dmaidecoder.h"
#include "rawbuffer.h"
#include "circularbuffer.h"
#define DEBUG
#include "emdesk/debug.h"
#include "xdc_config.h"

#include <ti/sdo/ce/Engine.h>
#include <errno.h>

/* TODO: Get rid of these flags(their names) */
#define gst_tidmaibuffer_GST_FREE        0x1
#define gst_tidmaibuffer_CODEC_FREE      0x2
#define gst_tidmaibuffer_VIDEOSINK_FREE  0x4
#define gst_tidmaibuffer_DISPLAY_FREE    0x8

static DmaiDecoder *instance = NULL;

DmaiDecoder::DmaiDecoder(QObject *parent) :
	BaseLmmDecoder(parent)
{
	hEngine = NULL;
	hCodec = NULL;
	instance = this;
}

DmaiDecoder::~DmaiDecoder()
{
	mDebug("deleting decoder %p", this);
	stopCodec();
	instance = NULL;
}

int DmaiDecoder::startDecoding()
{
	int err = startCodec();
	if (!err) {
		Buffer_Attrs attrs = Buffer_Attrs_DEFAULT;
		circBufData = Buffer_create(Vdec2_getInBufSize(hCodec) * 3, &attrs);
		circBuf = new CircularBuffer(Buffer_getUserPtr(circBufData)
									 , Buffer_getSize(circBufData));
	} else {
		mDebug("cannot start codec");
		circBufData = NULL;
		circBuf = NULL;
	}
	timestamp = NULL;
	decodeCount = 0;
	return err;
}

int DmaiDecoder::stopDecoding()
{
	int err = stopCodec();
	if (circBufData) {
		Buffer_delete(circBufData);
		circBufData = NULL;
	}
	if (circBuf) {
		delete circBuf;
		circBuf = NULL;
	}

	return err;
}

int DmaiDecoder::decodeOne()
{
	RawBuffer *buf = NULL;
	while (inputBuffers.size()) {
		bool decodeOk = true;
		buf = inputBuffers.takeFirst();
		if (!buf && circBuf->usedSize() == 0)
			return -ENOENT;
		if (buf) {
			mInfo("adding %d bytes to circular buffer", buf->size());
			if (circBuf->addData(buf->data(), buf->size()))
				mDebug("error adding data to circular buffer");
			handleInputTimeStamps(buf);
			delete buf;
		}
		Buffer_Handle hBuf = BufTab_getFreeBuf(hBufTab);
		if (!hBuf) {
			/*
			 * This is not an error, probably buffers are not displayed yet
			 * and we don't need to decode any more
			 */
			mInfo("cannot get new buf from buftab");
			return -ENOENT;
		}

		/* Make sure the whole buffer is used for output */
		BufferGfx_resetDimensions(hBuf);

		/* Create an input buffer for decoder, reference circular buffer data */
		Buffer_Attrs bAttrs;
		Buffer_getAttrs(circBufData, &bAttrs);
		bAttrs.reference = TRUE;
		Buffer_Handle hCircBufWindow = Buffer_create(circBuf->usedSize(), &bAttrs);
		Buffer_setUserPtr(hCircBufWindow, (Int8 *)circBuf->getDataPointer());
		Buffer_setNumBytesUsed(hCircBufWindow, circBuf->usedSize());

		mInfo("invoking the video decoder with data size %d", circBuf->usedSize());
		int ret = Vdec2_process(hCodec, hCircBufWindow, hBuf);
		int consumed = Buffer_getNumBytesUsed(hCircBufWindow);
		if (ret < 0) {
			if (consumed <= 0)
				consumed = 1;
			BufTab_freeBuf(hBuf);
			mDebug("failed to decode frame");
			decodeOk = false;
		} else if (ret > 0) {
			mDebug("frame decoded with success code %d", ret);
			if (ret == Dmai_EBITERROR) {
				BufTab_freeBuf(hBuf);
				if (consumed == 0)
					mDebug("fatal dmai bit error");
			}
		}
		Buffer_delete(hCircBufWindow);
		mInfo("decoder used %d bytes out of %d", consumed, circBuf->usedSize());
		if (circBuf->useData(consumed) != consumed)
			mDebug("something wrong with circ buffer");
		if (!decodeOk)
			break;
		Buffer_Handle outbuf = Vdec2_getDisplayBuf(hCodec);
		if (outbuf) {
			BufferGfx_Dimensions dim;
			BufferGfx_getDimensions(outbuf, &dim);
			mInfo("decoded frame width=%d height=%d", int(dim.width), (int)dim.height);
			RawBuffer *newbuf = new RawBuffer;
			newbuf->setRefData(Buffer_getUserPtr(outbuf), Buffer_getSize(outbuf));
			newbuf->addBufferParameter("width", (int)dim.width);
			newbuf->addBufferParameter("height", (int)dim.height);
			newbuf->addBufferParameter("dmaiBuffer", (int)outbuf);
			newbuf->setStreamBufferNo(decodeCount++);

			/* handle timestamps */
			setOutputTimeStamp(newbuf);

			outputBuffers << newbuf;
			/* set the resulting buffer in use by video output */
			Buffer_setUseMask(outbuf, Buffer_getUseMask(outbuf) | gst_tidmaibuffer_VIDEOSINK_FREE);
		} else
			mDebug("unable to find a free display buffer");
		/* Release buffers no longer in use by the codec */
		outbuf = Vdec2_getFreeBuf(hCodec);
		while (outbuf) {
			Buffer_freeUseMask(outbuf, gst_tidmaibuffer_CODEC_FREE);
			outbuf = Vdec2_getFreeBuf(hCodec);
		}
		break;
	}

	return 0;
}

int DmaiDecoder::flush()
{
	circBuf->reset();
	/*
	 * Note: Do not flush dsp codec here, it is not needed and
	 * it causes more problems. i.e. buffer display order switching
	 */
	return BaseLmmDecoder::flush();
}

void DmaiDecoder::initCodecEngine()
{
	CERuntime_init();
	Dmai_init();
}

void DmaiDecoder::cleanUpDsp()
{
	if (instance)
		delete instance;
}

int DmaiDecoder::startCodec()
{
	VIDDEC2_Params params = Vdec2_Params_DEFAULT;
	VIDDEC2_DynamicParams dynParams = Vdec2_DynamicParams_DEFAULT;
	BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
	Cpu_Device device;
	ColorSpace_Type colorSpace;
	int defaultNumBufs;

	const char *engineName = "decode";
	/* Open the codec engine */
	mDebug("opening codec engine \"%s\"", engineName);
	hEngine = Engine_open((char *)engineName, NULL, NULL);

	if (hEngine == NULL) {
		mDebug("failed to open codec engine \"%s\"", engineName);
		return -ENOENT;
	}

	/* Determine which device the application is running on */
	if (Cpu_getDevice(NULL, &device) < 0) {
		mDebug("Failed to determine target CPU, assuming DM6446");
		device = Cpu_Device_DM6446;
	}

	/* Set up codec parameters depending on device */
	switch(device) {
	case Cpu_Device_DM6467:
#if defined(Platform_dm6467t)
		params.forceChromaFormat = XDM_YUV_420SP;
		params.maxFrameRate      = 60000;
		params.maxBitRate        = 30000000;
#else
		params.forceChromaFormat = XDM_YUV_420P;
#endif
		params.maxWidth          = VideoStd_1080I_WIDTH;
		params.maxHeight         = VideoStd_1080I_HEIGHT + 8;
		colorSpace               = ColorSpace_YUV420PSEMI;
		defaultNumBufs           = 5;
		break;
#if defined(Platform_dm365)
	case Cpu_Device_DM365:
		params.forceChromaFormat = XDM_YUV_420SP;
		params.maxWidth          = VideoStd_720P_WIDTH;
		params.maxHeight         = VideoStd_720P_HEIGHT;
		colorSpace               = ColorSpace_YUV420PSEMI;
		defaultNumBufs           = 4;
		break;
#endif
#if defined(Platform_dm368)
	case Cpu_Device_DM368:
		params.forceChromaFormat = XDM_YUV_420SP;
		params.maxWidth          = VideoStd_720P_WIDTH;
		params.maxHeight         = VideoStd_720P_HEIGHT;
		colorSpace               = ColorSpace_YUV420PSEMI;
		defaultNumBufs           = 4;
		break;
#endif
#if defined(Platform_omapl138)
	case Cpu_Device_OMAPL138:
		params.forceChromaFormat = XDM_YUV_420P;
		params.maxWidth          = VideoStd_D1_WIDTH;
		params.maxHeight         = VideoStd_D1_PAL_HEIGHT;
		colorSpace               = ColorSpace_YUV420P;
		defaultNumBufs           = 3;
		break;
#endif
#if defined(Platform_dm3730)
	case Cpu_Device_DM3730:
		params.maxWidth          = VideoStd_720P_WIDTH;
		params.maxHeight         = VideoStd_720P_HEIGHT;
		params.forceChromaFormat = XDM_YUV_422ILE;
		colorSpace               = ColorSpace_UYVY;
		defaultNumBufs           = 3;
		break;
#endif
	default:
		params.forceChromaFormat = XDM_YUV_422ILE;
		params.maxWidth          = VideoStd_D1_WIDTH;
		params.maxHeight         = VideoStd_D1_PAL_HEIGHT;
		colorSpace               = ColorSpace_UYVY;
		defaultNumBufs           = 3;
		break;
	}

	const char *codecName = "mpeg4dec";
	mDebug("opening video decoder \"%s\"", codecName);
	hCodec = Vdec2_create(hEngine, (char *)codecName, &params, &dynParams);
	if (hCodec == NULL) {
		mDebug("failed to create video decoder: %s",codecName);
		return -EINVAL;
	}

	numOutputBufs = defaultNumBufs;
	mDebug("creating output buffer table");
	gfxAttrs.colorSpace = colorSpace;
	gfxAttrs.dim.width = params.maxWidth;
	gfxAttrs.dim.height = params.maxHeight;
	gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(gfxAttrs.dim.width, gfxAttrs.colorSpace);
	/* TODO: By default, new buffers are marked as in-use by the codec */
	gfxAttrs.bAttrs.useMask = gst_tidmaibuffer_CODEC_FREE;
	hBufTab = BufTab_create(numOutputBufs, Vdec2_getOutBufSize(hCodec), BufferGfx_getBufferAttrs(&gfxAttrs));
	if (hBufTab == NULL) {
		mDebug("no BufTab available for codec output");
		return -ENOMEM;
	}
	Vdec2_setBufTab(hCodec, hBufTab);

	return 0;
}

int DmaiDecoder::stopCodec()
{
	/* Shut down remaining items */
	if (hCodec) {
		Vdec2_flush(hCodec);
		mDebug("closing video decoder");
		Vdec2_delete(hCodec);
		hCodec = NULL;
	}

	if (hEngine) {
		mDebug("closing codec engine");
		Engine_close(hEngine);
		hEngine = NULL;
	}

	return 0;
}
