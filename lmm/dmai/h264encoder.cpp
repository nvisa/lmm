#include "h264encoder.h"
#include "dmaiencoder.h"
#include "dmai/dmaibuffer.h"
#include "debug.h"
#include "dm365/ih264venc.h"
#include "streamtime.h"
#include "tools/cpuload.h"
#include "h264parser.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/video1/videnc1.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/ColorSpace.h>
#include <ti/sdo/dmai/ce/Venc1.h>
#include <ti/sdo/dmai/BufferGfx.h>

#include <errno.h>
#include <math.h>

#include <QDateTime>

extern VUIParamBuffer H264VENC_TI_VUIPARAMBUFFER;

static void printErrorMsg(XDAS_Int32 errorCode)
{
	if(0 == errorCode)
		return;

	printf("\n");
	printf("CODEC_DEBUG_ENABLE:");
	if(XDM_ISFATALERROR(errorCode))
	{
		printf("\tFatal Error");
	}

	if(XDM_ISUNSUPPORTEDPARAM(errorCode))
	{
		printf("\tUnsupported Param Error");
	}

	if(XDM_ISUNSUPPORTEDINPUT(errorCode))
	{
		printf("\tUnsupported Input Error");
	}
	printf(":\t");

	switch(errorCode)
	{
	case IH264VENC_ERR_MAXWIDTH :
		printf("IH264VENC_ERR_MAXWIDTH\n");
		break;

	case IH264VENC_ERR_MAXHEIGHT :
		printf("IH264VENC_ERR_MAXHEIGHT\n");
		break;

	case IH264VENC_ERR_ENCODINGPRESET:
		printf("IH264VENC_ERR_ENCODINGPRESET \n");
		break;

	case IH264VENC_ERR_RATECONTROLPRESET:
		printf("IH264VENC_ERR_RATECONTROLPRESET \n");
		break;

	case IH264VENC_ERR_MAXFRAMERATE:
		printf("IH264VENC_ERR_MAXFRAMERATE \n");
		break;

	case IH264VENC_ERR_MAXBITRATE:
		printf("IH264VENC_ERR_MAXBITRATE \n");
		break;

	case IH264VENC_ERR_DATAENDIANNESS:
		printf("IH264VENC_ERR_DATAENDIANNESS \n");
		break;

	case IH264VENC_ERR_INPUTCHROMAFORMAT:
		printf("IH264VENC_ERR_INPUTCHROMAFORMAT \n");
		break;

	case IH264VENC_ERR_INPUTCONTENTTYPE:
		printf("IH264VENC_ERR_INPUTCONTENTTYPE \n");
		break;

	case IH264VENC_ERR_RECONCHROMAFORMAT:
		printf("IH264VENC_ERR_RECONCHROMAFORMAT \n");
		break;

	case IH264VENC_ERR_INPUTWIDTH :
		printf("IH264VENC_ERR_INPUTWIDTH\n");
		break;

	case IH264VENC_ERR_INPUTHEIGHT :
		printf("IH264VENC_ERR_INPUTHEIGHT\n");
		break;

	case IH264VENC_ERR_MAX_MBS_IN_FRM_LIMIT_EXCEED:
		printf("IH264VENC_ERR_MAX_MBS_IN_FRM_LIMIT_EXCEED \n");
		break;

	case IH264VENC_ERR_TARGETFRAMERATE:
		printf("IH264VENC_ERR_TARGETFRAMERATE \n");
		break;

	case IH264VENC_ERR_TARGETBITRATE:
		printf("IH264VENC_ERR_TARGETBITRATE \n");
		break;

	case IH264VENC_ERR_PROFILEIDC :
		printf("IH264VENC_ERR_PROFILEIDC\n");
		break;

	case IH264VENC_ERR_LEVELIDC :
		printf("IH264VENC_ERR_LEVELIDC\n");
		break;

	case IH264VENC_ERR_ENTROPYMODE_IN_BP :
		printf("IH264VENC_ERR_ENTROPYMODE_IN_BP\n");
		break;

	case IH264VENC_ERR_TRANSFORM8X8FLAGINTRA_IN_BP_MP :
		printf("IH264VENC_ERR_TRANSFORM8X8FLAGINTRA_IN_BP_MP\n");
		break;

	case IH264VENC_ERR_TRANSFORM8X8FLAGINTER_IN_BP_MP :
		printf("IH264VENC_ERR_TRANSFORM8X8FLAGINTER_IN_BP_MP\n");
		break;

	case IH264VENC_ERR_SEQSCALINGFLAG_IN_BP_MP :
		printf("IH264VENC_ERR_SEQSCALINGFLAG_IN_BP_MP\n");
		break;

	case IH264VENC_ERR_ASPECTRATIOX :
		printf("IH264VENC_ERR_ASPECTRATIOX\n");
		break;

	case IH264VENC_ERR_ASPECTRATIOY :
		printf("IH264VENC_ERR_ASPECTRATIOY\n");
		break;

	case IH264VENC_ERR_PIXELRANGE :
		printf("IH264VENC_ERR_PIXELRANGE\n");
		break;

	case IH264VENC_ERR_TIMESCALE :
		printf("IH264VENC_ERR_TIMESCALE\n");
		break;

	case IH264VENC_ERR_NUMUNITSINTICKS :
		printf("IH264VENC_ERR_NUMUNITSINTICKS\n");
		break;

	case IH264VENC_ERR_ENABLEVUIPARAMS :
		printf("IH264VENC_ERR_ENABLEVUIPARAMS\n");
		break;

	case IH264VENC_ERR_RESETHDVICPEVERYFRAME :
		printf("IH264VENC_ERR_RESETHDVICPEVERYFRAME\n");
		break;

	case IH264VENC_ERR_MEALGO :
		printf("IH264VENC_ERR_MEALGO\n");
		break;

    case IH264VENC_ERR_UNRESTRICTEDMV :
		printf("IH264VENC_ERR_UNRESTRICTEDMV\n");
		break;

	case IH264VENC_ERR_ENCQUALITY :
		printf("IH264VENC_ERR_ENCQUALITY\n");
		break;

	case IH264VENC_ERR_ENABLEARM926TCM :
		printf("IH264VENC_ERR_ENABLEARM926TCM\n");
		break;

	case IH264VENC_ERR_ENABLEDDRBUFF :
		printf("IH264VENC_ERR_ENABLEDDRBUFF\n");
		break;

	case IH264VENC_ERR_LEVEL_NOT_FOUND :
		printf("IH264VENC_ERR_LEVEL_NOT_FOUND\n");
		break;

	case IH264VENC_ERR_REFFRAMERATE_MISMATCH :
		printf("IH264VENC_ERR_REFFRAMERATE_MISMATCH\n");
		break;

	case IH264VENC_ERR_INTRAFRAMEINTERVAL :
		printf("IH264VENC_ERR_INTRAFRAMEINTERVAL\n");
		break;

	case IH264VENC_ERR_GENERATEHEADER :
		printf("IH264VENC_ERR_GENERATEHEADER\n");
		break;

	case IH264VENC_ERR_FORCEFRAME :
		printf("IH264VENC_ERR_FORCEFRAME\n");
		break;

	case IH264VENC_ERR_RCALGO :
		printf("IH264VENC_ERR_RCALGO\n");
		break;

	case IH264VENC_ERR_INTRAFRAMEQP :
		printf("IH264VENC_ERR_INTRAFRAMEQP\n");
		break;

	case IH264VENC_ERR_INTERPFRAMEQP :
		printf("IH264VENC_ERR_INTERPFRAMEQP\n");
		break;

	case IH264VENC_ERR_RCQMAX :
		printf("IH264VENC_ERR_RCQMAX\n");
		break;

	case IH264VENC_ERR_RCQMIN :
		printf("IH264VENC_ERR_RCQMIN\n");
		break;

	case IH264VENC_ERR_RCIQMAX :
		printf("IH264VENC_ERR_RCIQMAX\n");
		break;

	case IH264VENC_ERR_RCIQMIN :
		printf("IH264VENC_ERR_RCIQMIN\n");
		break;

	case IH264VENC_ERR_INITQ :
		printf("IH264VENC_ERR_INITQ\n");
		break;

	case IH264VENC_ERR_MAXDELAY :
		printf("IH264VENC_ERR_MAXDELAY\n");
		break;

	case IH264VENC_ERR_LFDISABLEIDC :
		printf("IH264VENC_ERR_LFDISABLEIDC\n");
		break;

	case IH264VENC_ERR_ENABLEBUFSEI :
		printf("IH264VENC_ERR_ENABLEBUFSEI\n");
		break;

	case IH264VENC_ERR_ENABLEPICTIMSEI :
		printf("IH264VENC_ERR_ENABLEPICTIMSEI\n");
		break;

	case IH264VENC_ERR_SLICESIZE :
		printf("IH264VENC_ERR_SLICESIZE\n");
		break;

	case IH264VENC_ERR_INTRASLICENUM :
		printf("IH264VENC_ERR_INTRASLICENUM\n");
		break;

	case IH264VENC_ERR_AIRRATE :
		printf("IH264VENC_ERR_AIRRATE\n");
		break;

	case IH264VENC_ERR_MEMULTIPART :
		printf("IH264VENC_ERR_MEMULTIPART\n");
		break;

	case IH264VENC_ERR_INTRATHRQF :
		printf("IH264VENC_ERR_INTRATHRQF\n");
		break;

	case IH264VENC_ERR_PERCEPTUALRC :
		printf("IH264VENC_ERR_PERCEPTUALRC\n");
		break;

	case IH264VENC_ERR_IDRFRAMEINTERVAL :
		printf("IH264VENC_ERR_IDRFRAMEINTERVAL\n");
		break;

	case IH264VENC_ERR_ENABLEROI :
		printf("IH264VENC_ERR_ENABLEROI\n");
		break;

	case IH264VENC_ERR_MAXBITRATE_CVBR :
		printf("IH264VENC_ERR_MAXBITRATE_CVBR\n");
		break;

    case IH264VENC_ERR_MVSADOUTFLAG :
		printf("IH264VENC_ERR_MVSADOUTFLAG\n");
		break;

	case IH264VENC_ERR_MAXINTERFRAMEINTERVAL :
		printf("IH264VENC_ERR_MAXINTERFRAMEINTERVAL\n");
		break;

	case IH264VENC_ERR_CAPTUREWIDTH :
		printf("IH264VENC_ERR_CAPTUREWIDTH\n");
		break;

	case IH264VENC_ERR_INTERFRAMEINTERVAL :
		printf("IH264VENC_ERR_INTERFRAMEINTERVAL\n");
		break;

	case IH264VENC_ERR_MBDATAFLAG :
		printf("IH264VENC_ERR_MBDATAFLAG\n");
		break;

	case IH264VENC_ERR_IVIDENC1_DYNAMICPARAMS_SIZE_IN_CORRECT :
		printf("IH264VENC_ERR_IVIDENC1_DYNAMICPARAMS_SIZE_IN_CORRECT\n");
		break;

	case IH264VENC_ERR_IVIDENC1_PROCESS_ARGS_NULL :
		printf("IH264VENC_ERR_IVIDENC1_PROCESS_ARGS_NULL\n");
		break;

	case IH264VENC_ERR_IVIDENC1_INARGS_SIZE :
		printf("IH264VENC_ERR_IVIDENC1_INARGS_SIZE\n");
		break;

	case IH264VENC_ERR_IVIDENC1_OUTARGS_SIZE :
		printf("IH264VENC_ERR_IVIDENC1_OUTARGS_SIZE\n");
		break;

	case IH264VENC_ERR_IVIDENC1_INARGS_INPUTID :
		printf("IH264VENC_ERR_IVIDENC1_INARGS_INPUTID\n");
		break;

	case IH264VENC_ERR_IVIDENC1_INARGS_TOPFIELDFIRSTFLAG :
		printf("IH264VENC_ERR_IVIDENC1_INARGS_TOPFIELDFIRSTFLAG\n");
		break;

	case IH264VENC_ERR_IVIDENC1_INBUFS :
		printf("IH264VENC_ERR_IVIDENC1_INBUFS\n");
		break;

	case IH264VENC_ERR_IVIDENC1_INBUFS_BUFDESC :
		printf("IH264VENC_ERR_IVIDENC1_INBUFS_BUFDESC\n");
		break;

	case IH264VENC_ERR_IVIDENC1_OUTBUFS :
		printf("IH264VENC_ERR_IVIDENC1_OUTBUFS\n");
		break;

	case IH264VENC_ERR_IVIDENC1_OUTBUFS_NULL :
		printf("IH264VENC_ERR_IVIDENC1_OUTBUFS_NULL\n");
		break;

	case IH264VENC_ERR_INTERLACE_IN_BP :
		printf("IH264VENC_ERR_INTERLACE_IN_BP\n");
		break;

	case IH264VENC_ERR_RESERVED :
		printf("IH264VENC_ERR_RESERVED\n");
		break;

	case IH264VENC_ERR_INSERTUSERDATA :
		printf("IH264VENC_ERR_INSERTUSERDATA\n");
		break;

	case IH264VENC_ERR_ROIPARAM :
		printf("IH264VENC_ERR_ROIPARAM\n");
		break;

	case IH264VENC_ERR_LENGTHUSERDATA :
		printf("IH264VENC_ERR_LENGTHUSERDATA\n");
		break;

	case IH264VENC_ERR_PROCESS_CALL :
		printf("IH264VENC_ERR_PROCESS_CALL\n");
		break;

	case IH264VENC_ERR_HANDLE_NULL :
		printf("IH264VENC_ERR_HANDLE_NULL\n");
		break;

	case IH264VENC_ERR_INCORRECT_HANDLE :
		printf("IH264VENC_ERR_INCORRECT_HANDLE\n");
		break;

	case IH264VENC_ERR_MEMTAB_NULL :
		printf("IH264VENC_ERR_MEMTAB_NULL\n");
		break;

	case IH264VENC_ERR_IVIDENC1_INITPARAMS_SIZE :
		printf("IH264VENC_ERR_IVIDENC1_INITPARAMS_SIZE\n");
		break;

	case IH264VENC_ERR_MEMTABS_BASE_NULL :
		printf("IH264VENC_ERR_MEMTABS_BASE_NULL\n");
		break;

	case IH264VENC_ERR_MEMTABS_BASE_NOT_ALIGNED :
		printf("IH264VENC_ERR_MEMTABS_BASE_NOT_ALIGNED\n");
		break;

	case IH264VENC_ERR_MEMTABS_SIZE :
		printf("IH264VENC_ERR_MEMTABS_SIZE\n");
		break;

	case IH264VENC_ERR_MEMTABS_ATTRS :
		printf("IH264VENC_ERR_MEMTABS_ATTRS\n");
		break;

	case IH264VENC_ERR_MEMTABS_SPACE :
		printf("IH264VENC_ERR_MEMTABS_SPACE\n");
		break;

	case IH264VENC_ERR_MEMTABS_OVERLAP :
		printf("IH264VENC_ERR_MEMTABS_OVERLAP\n");
		break;

	case IH264VENC_ERR_CODEC_INACTIVE :
		printf("IH264VENC_ERR_CODEC_INACTIVE\n");
		break;

	case IH264VENC_WARN_LEVELIDC :
		printf("IH264VENC_WARN_LEVELIDC\n");
		break;

	case IH264VENC_WARN_RATECONTROLPRESET :
		printf("IH264VENC_WARN_RATECONTROLPRESET\n");
		break;

	case IH264VENC_WARN_H241_SLICE_SIZE_EXCEEDED :
		printf("IH264VENC_WARN_H241_SLICE_SIZE_EXCEEDED\n");
		break;

	case IH264VENC_ERR_STATUS_BUF :
		printf("IH264VENC_ERR_STATUS_BUF\n");
		break;

	case IH264VENC_ERR_METADATAFLAG :
		printf("IH264VENC_ERR_METADATAFLAG\n");
		break;

	default:
		printf("Unknown Error code\n");
	}
}

H264Encoder::H264Encoder(QObject *parent) :
	DmaiEncoder(parent)
{
	setDynamicParams = false;
	seiEnabled = false;
	dirty = false;
	encodeFps = 30;
	profileId = 0;
	useMetadata = false;
	genMetadata = false;
	frameinfoInterface = NULL;
	mVecs = MV_NONE;
	pmod = PMOD_PACKETIZED;
	motionDetectionThresh = 0;
	motionValue = 0;
	enablePictureTimingSei(true);

	setLockUpFixLockerType(2);
}

int H264Encoder::flush()
{
	return DmaiEncoder::flush();
}

/**
 * @brief H264Encoder::isPacketized
 * @return
 *
 * \sa setPacketized()
 */
bool H264Encoder::isPacketized()
{
	return pmod & PMOD_PACKETIZED;
}

/**
 * @brief H264Encoder::setPacketized
 * @param value
 *
 * This function can be used to adjust encoder to generate packetized
 * output buffers. When encoder is packetized, it outputs a new buffer
 * for each NAL unit created. Otherwise, it outputs one buffer for each
 * incoming video frame. Packetized output is necessary for proper
 * SEI insertion, SEI user data may contain byte sequence of '00000001'
 * which will be interpreted as a new NAL unit by upstream packetizers
 * (for instance RTP packetizers) which will produce wrong results.
 *
 * If SEI insertion is not used then packetized mode can be switched-off
 * for performance reasons because there is a little performance overhead
 * in the encoder due to packetization.
 *
 * Note that when you receive packetized output, you don't need to copy
 * buffers in upstream elements because encoder copies them. But when
 * you are not using packetized output you need to copy buffers(or use
 * them as soon as possible) so that encoders can re-use buffers when
 * more frames arrive.
 */
void H264Encoder::setPacketized(bool value)
{
	if (value)
		pmod = PMOD_PACKETIZED;
	else
		pmod = PMOD_NORMAL;
}

void H264Encoder::setPacketizationMode(H264Encoder::PacketizationMode mod)
{
	pmod = mod;
}

int H264Encoder::enablePictureTimingSei(bool enable)
{
	if (enable) {
		enableBufSei = 1;
		enableVUIParams = 4;
		enablePicTimSei = 1;
	} else {
		enableBufSei = 0;
		enableVUIParams = 0;
		enablePicTimSei = 0;
	}
	return 0;
}

int H264Encoder::useGeneratedMetadata(bool v)
{
	useMetadata = v;
	return 0;
}

int H264Encoder::generateMetadata(bool v)
{
	genMetadata = v;
	return 0;
}

void H264Encoder::setMetadata(void *data)
{
	if (data)
		memcpy(frameinfoInterface, data, sizeof(FrameInfo_Interface));
}

void H264Encoder::setFrameRate(float fps)
{
	DmaiEncoder::setFrameRate(fps);
	encodeFps = fps;
}

void H264Encoder::setSeiEnabled(bool value)
{
	seiEnabled = value;
}

void H264Encoder::setMotionDetectionThreshold(int th)
{
	motionDetectionThresh = th;
}

int H264Encoder::getMotionValue()
{
	return motionValue;
}

typedef struct Venc1_Object {
	VIDENC1_Handle          hEncode;
	IVIDEO1_BufDesc         reconBufs;
	Int32                   minNumInBufs;
	Int32                   minInBufSize[XDM_MAX_IO_BUFFERS];
	Int32                   minNumOutBufs;
	Int32                   minOutBufSize[XDM_MAX_IO_BUFFERS];
	BufTab_Handle           hInBufTab;
	Buffer_Handle           hFreeBuf;
	VIDENC1_DynamicParams   dynH264Params;
} Venc1_Object;
/* As 0 is not a valid buffer id in XDM 1.0, we need macros for easy access */
#define GETID(x)  ((x) + 1)
#define GETIDX(x) ((x) - 1)

static Int Venc1_processL(Venc1_Handle hVe, Buffer_Handle hInBuf, Buffer_Handle hOutBuf, int seiDataSize, int *seiDataOffset
						  , int timeStamp, struct ROI_Interface *roi, bool genFinf, void *finf, Buffer_Handle *finfg,
						  bool genMotionVectors)
{
	IVIDEO1_BufDescIn       inBufDesc;
	XDM_BufDesc             outBufDesc;
	XDAS_Int32              outBufSizeArray[3];
	XDAS_Int32              status;
	IH264VENC_InArgs		inArgs;
	IH264VENC_OutArgs       outArgs;
	XDAS_Int8              *inPtr;
	XDAS_Int8              *outPtr[4];
	BufferGfx_Dimensions    dim;
	Int32                  offset = 0;
	Uint32                  bpp;

	bpp = ColorSpace_getBpp(BufferGfx_getColorSpace(hInBuf));

	BufferGfx_getDimensions(hInBuf, &dim);

	offset = (dim.y * dim.lineLength) + (dim.x * (bpp >> 3));
	assert(offset < Buffer_getSize(hInBuf));

	inPtr  = Buffer_getUserPtr(hInBuf)  + offset;
	outPtr[0] = Buffer_getUserPtr(hOutBuf);

	/* Set up the codec buffer dimensions */
	inBufDesc.frameWidth                = dim.width;
	inBufDesc.frameHeight               = dim.height;
	inBufDesc.framePitch                = dim.lineLength;

	/* Point to the color planes depending on color space format */
	if (BufferGfx_getColorSpace(hInBuf) == ColorSpace_YUV420PSEMI) {
		inBufDesc.bufDesc[0].bufSize    = hVe->minInBufSize[0];
		inBufDesc.bufDesc[1].bufSize    = hVe->minInBufSize[1];

		inBufDesc.bufDesc[0].buf        = inPtr;
		inBufDesc.bufDesc[1].buf        = inPtr + Buffer_getSize(hInBuf) * 2/3;
		inBufDesc.numBufs               = 2;

		if (finf) {
			inBufDesc.bufDesc[2].buf		= (XDAS_Int8 *)finf;
			inBufDesc.numBufs++;
		}
	}
	else if (BufferGfx_getColorSpace(hInBuf) == ColorSpace_UYVY) {
		inBufDesc.bufDesc[0].bufSize    = Buffer_getSize(hInBuf);
		inBufDesc.bufDesc[0].buf        = inPtr;
		inBufDesc.numBufs               = 1;
	}
	else {
		fDebug("Unsupported color format of input buffer");
		return Dmai_EINVAL;
	}

	outBufSizeArray[0]                  = Buffer_getSize(hOutBuf);

    outBufDesc.numBufs                  = 1;
    outBufDesc.bufs	                    = outPtr;
    outBufDesc.bufSizes                 = outBufSizeArray;

	if (genFinf) {
		outBufSizeArray[1]                  = Buffer_getSize(finfg[0]);
		outPtr[1]							= Buffer_getUserPtr(finfg[0]);
		outBufDesc.numBufs					+= 1;
	}

	if (genMotionVectors) {
		outBufSizeArray[1]                  = Buffer_getSize(finfg[1]);
		outPtr[1]							= Buffer_getUserPtr(finfg[1]);
		outBufDesc.numBufs					+= 1;
	}

	inArgs.videncInArgs.size                         = sizeof(IH264VENC_InArgs);
	inArgs.videncInArgs.inputID                      = GETID(Buffer_getId(hInBuf));

	/* topFieldFirstFlag is hardcoded. Used only for interlaced content */
	inArgs.videncInArgs.topFieldFirstFlag            = 1;

	/* h264 specific parameters */
	inArgs.lengthUserData = seiDataSize;
	if (inArgs.lengthUserData)
		inArgs.insertUserData = 1;
	else
		inArgs.insertUserData = 0;
	inArgs.timeStamp = timeStamp;
	inArgs.roiParameters.numOfROI = 0;
	if (roi)
		inArgs.roiParameters = *roi;

	outArgs.videncOutArgs.size = sizeof(IH264VENC_OutArgs);

	/* Encode video buffer */
	status = VIDENC1_process(hVe->hEncode, &inBufDesc, &outBufDesc, (VIDENC1_InArgs *)&inArgs,
							 (VIDENC1_OutArgs *)&outArgs);

	//fDebug("VIDENC1_process() ret %d inId %d outID %d generated %d bytes",
		//   (int)status, Buffer_getId(hInBuf), (int)outArgs.outputID, (int)outArgs.bytesGenerated);

	if (status != VIDENC1_EOK) {
		fDebug("VIDENC1_process() failed with error (%d ext: 0x%x)",
			   (Int)status, (Uns) outArgs.videncOutArgs.extendedError);
		return Dmai_EFAIL;
	}

	/* if memTab was used for input buffers */
	if(hVe->hInBufTab != NULL) {
		/* One buffer is freed when output content is generated */
		if(outArgs.videncOutArgs.bytesGenerated>0) {
			/* Buffer released by the encoder */
			hVe->hFreeBuf = BufTab_getBuf(hVe->hInBufTab, GETIDX(outArgs.videncOutArgs.outputID));
		}
		else {
			hVe->hFreeBuf = NULL;
		}
	}

	/* Copy recon buffer information so we can retrieve it later */
	hVe->reconBufs = outArgs.videncOutArgs.reconBufs;

	/*
	 * Setting the frame type in the input buffer even through it's a property
	 * of the output buffer. This works for encoders without B-frames, but
	 * when byte and display order are not the same the frame type will get
	 * out of sync.
	 */
	BufferGfx_setFrameType(hInBuf, outArgs.videncOutArgs.encodedFrameType);

	/* mark used buffer size */
	Buffer_setNumBytesUsed(hOutBuf, outArgs.videncOutArgs.bytesGenerated);

	if (seiDataSize)
		*seiDataOffset = outArgs.offsetUserData;
	else
		*seiDataOffset = 0;

	return Dmai_EOK;
}

static void dump_buffer(char *buffer, int len)
{
	for (int i = 0; i < len; i += 16) {
		for (int j = 0; j < 16; j++)
			printf("0x%x,", buffer[i + j]);
		printf("\n");
	}
}

int H264Encoder::encode(Buffer_Handle buffer, const RawBuffer source)
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
		return -EAGAIN;
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
		status.size = sizeof(IH264VENC_Status);
		dynH264Params->videncDynamicParams.forceFrame = IVIDEO_IDR_FRAME;
		if (VIDENC1_control(Venc1_getVisaHandle(hCodec), XDM_SETPARAMS,
							&dynH264Params->videncDynamicParams, &status) != VIDENC1_EOK) {
			qDebug("error setting control on encoder: 0x%x", (int)status.extendedError);
			printErrorMsg(status.extendedError);
		} else
			idrGenerated = true;
	}

	if (setDynamicParams) {
		mDebug("updating dynamic params");
		VIDENC1_Status status;
		status.size = sizeof(IH264VENC_Status);
		if (VIDENC1_control(Venc1_getVisaHandle(hCodec), XDM_SETPARAMS,
							&dynH264Params->videncDynamicParams, &status) != VIDENC1_EOK) {
			qDebug("error setting control on encoder: 0x%x", (int)status.extendedError);
			printErrorMsg(status.extendedError);
		}
		setDynamicParams = false;
	}

	mInfo("invoking venc1_process: width=%d height=%d",
		  (int)dim.width, (int)dim.height);
	/* Encode the video buffer */
	int seiDataOffset;
	if (useMetadata)
		mDebug("multi-channel encoding video height is %d", ((FrameInfo_Interface *)frameinfoInterface)->hight);

	/*
	 * we overlay previous motion vectors on the next frame if motion
	 * vector overlay is desired
	 */
	if (mVecs == MV_OVERLAY || mVecs == MV_BOTH) {
		int w = source.constPars()->videoWidth;
		int h = source.constPars()->videoHeight;
		uchar *vdata = (uchar *)source.constData();
		uchar motVecBuf[w / 16 * h / 16];
		uchar *motVectBuf = (uchar *)Buffer_getUserPtr(metadataBuf[1]);
		qint16 *vbuf16 = (qint16 *)motVectBuf;
		qint32 *vbuf32 = (qint32 *)motVectBuf;
		for (int j = 0; j < imageHeight / 16; j++) {
			int off = j * imageWidth / 16;
			for (int i = 0; i < imageWidth / 16; i++) {
				short hd = vbuf16[off * 4 + i * 4];
				short vd = vbuf16[off * 4 + i * 4 + 1];
				int sad = vbuf32[off * 2 + i * 2 + 1];
				motVecBuf[off + i] = sqrt(hd * hd + vd * vd);
				if (motVecBuf[off + i] < motionDetectionThresh)
					motVecBuf[off + i] = 0;
			}
		}
		/* show motion vectors */
		for (int i = 0; i < w / 16; i++) {
			for (int j = 0; j < h / 16; j++) {
				int tmp = motVecBuf[j * w / 16 + i];
				vdata[j * w + i] =  tmp > 16 ? tmp : 0;
			}
		}
	}
	int seiBufferSize = 0;
	if (seiEnabled)
		seiBufferSize = source.constPars()->metaData.size() + 20;
	if (Venc1_processL(hCodec, buffer, hDstBuf, seiBufferSize, &seiDataOffset,
					   timeStamp, roiParameter((uchar *)Buffer_getUserPtr(buffer)),
					   genMetadata, useMetadata ? frameinfoInterface : NULL, metadataBuf,
					   mVecs != MV_NONE) < 0) {
		mDebug("Failed to encode video buffer");
		mDebug("width=%d height=%d imageWidth=%d imageHeight=%d seiEnabled=%d videoWidth=%d videoHeight=%d buffer=%p",
			(int)dim.width, (int)dim.height,
			imageWidth, imageHeight, seiEnabled, source.constPars()->videoWidth, source.constPars()->videoHeight, buffer);
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
	if (genMetadata) {
		memcpy(frameinfoInterface, Buffer_getUserPtr(metadataBuf[0]), sizeof(FrameInfo_Interface));
		mDebug("multi-channel encoding video width is %d", ((FrameInfo_Interface *)frameinfoInterface)->width);
	}
	if (enablePicTimSei)
		timeStamp++;
	if (idrGenerated) {
		VIDENC1_Status status;
		status.size = sizeof(IH264VENC_Status);
		dynH264Params->videncDynamicParams.forceFrame = IVIDEO_NA_FRAME;
		if (VIDENC1_control(Venc1_getVisaHandle(hCodec), XDM_SETPARAMS,
							&dynH264Params->videncDynamicParams, &status) != VIDENC1_EOK)
			qDebug("error setting control on encoder: 0x%x", (int)status.extendedError);
		generateIdrFrame = false;
	}

	if (mVecs == MV_MOTDET) {
		QElapsedTimer tm; tm.start();
		uchar *motVectBuf = (uchar *)Buffer_getUserPtr(metadataBuf[1]);
		qint16 *vbuf16 = (qint16 *)motVectBuf;
		int sum = 0;
		if (BufferGfx_getFrameType(buffer) == IVIDEO_IDR_FRAME ||
				BufferGfx_getFrameType(buffer) == IVIDEO_I_FRAME) {
			/*
			 * No need to calculate motion for I-frames, it will be 0 anyways.
			 * On the other hand, some motion parameters can be adjusted depending
			 * on the I-frame size [TODO].
			*/
		} else {
			for (int j = 0; j < imageHeight / 16; j++) {
				int off = j * imageWidth / 16;
				for (int i = 0; i < imageWidth / 16; i++) {
					short hd = vbuf16[off * 4 + i * 4];
					short vd = vbuf16[off * 4 + i * 4 + 1];
					sum += qAbs(hd) + qAbs(vd);
				}
			}
		}
		motionValue = sum;
	} else
		motionValue = 0;

	uint duration = 1000 / (float)source.constPars()->fps;
	if (pmod & PMOD_PACKETIZED) {
		QList<int> offsets;
		QList<int> nals;
		uchar *encdata = (uchar *)Buffer_getUserPtr(hDstBuf);
		uchar *encend = (uchar *)Buffer_getUserPtr(hDstBuf) + Buffer_getNumBytesUsed(hDstBuf);
		const uchar *nal = SimpleH264Parser::findNextStartCode(encdata, encend);
		while (1) {
			offsets << nal - encdata;
			int naltype = SimpleH264Parser::getNalType(nal);
			nals << naltype;
			if (naltype <= 5)
				break;
			nal = SimpleH264Parser::findNextStartCode(nal + 4, encend);
			if (nal >= encend - 4)
				break;
		}
		qint64 etime = streamTime->getCurrentTime();
		QList<RawBuffer> list;
		for (int i = 0; i < offsets.size(); i++) {
			int start = offsets[i];
			int end = encend - encdata;
			if (i < offsets.size() - 1)
				end = offsets[i + 1];
			RawBuffer buf = RawBuffer("video/x-h264", end - start);
			buf.setParameters(source.constPars());
			buf.pars()->frameType = BufferGfx_getFrameType(buffer);
			buf.pars()->encodeTime = etime;
			buf.pars()->streamBufferNo = encodeCount;
			buf.pars()->duration = duration;
			buf.pars()->captureTime = source.constPars()->captureTime;
			if (seiDataOffset < end && seiDataOffset > start)
				insertSeiData(seiDataOffset, hDstBuf, source);
			memcpy(buf.data(), encdata + start, end - start);
			buf.pars()->h264NalType = nals[i];
			list << buf;
		}
		newOutputBuffer(0, list);
		/* Reset the dimensions to what they were originally */
		BufferGfx_resetDimensions(buffer);
		encodeCount++;
		BufTab_freeBuf(hDstBuf);
	}
	if (pmod & PMOD_NORMAL) {
		/* insert SEI data */
		if (seiDataOffset) {
			insertSeiData(seiDataOffset, hDstBuf, source);
		}
		RawBuffer buf = DmaiBuffer("video/x-h264", hDstBuf, this);
		buf.setParameters(source.constPars());
		buf.pars()->frameType = BufferGfx_getFrameType(buffer);
		buf.pars()->encodeTime = streamTime->getCurrentTime();
		buf.pars()->captureTime = source.constPars()->captureTime;
		buf.pars()->streamBufferNo = encodeCount;
		buf.pars()->duration = duration;
		buf.pars()->dmaiBuffer = (quintptr *)hDstBuf;
		Buffer_setUseMask(hDstBuf, Buffer_getUseMask(hDstBuf) | 0x1);
		/* Reset the dimensions to what they were originally */
		BufferGfx_resetDimensions(buffer);
		if (pmod == PMOD_BOTH)
			newOutputBuffer(1, buf);
		else
			newOutputBuffer(0, buf);
	}

	return 0;
}

int H264Encoder::insertSeiData(int seiDataOffset, Buffer_Handle hDstBuf, RawBuffer source)
{
	int seiBufferSize = source.constPars()->metaData.size() + 20;
	mInfo("inserting %d bytes sei user data at offset %d, total size is %d",
		   seiBufferSize, seiDataOffset, (int)Buffer_getNumBytesUsed(hDstBuf));
	char *seidata = (char *)Buffer_getUserPtr(hDstBuf) + seiDataOffset;
	/* add IEC 11578 uuid */
	for (int i = 0; i < 16; i++)
		seidata[i] = 0xAA;
	seidata[16] = seiBufferSize & 0xff;
	seidata[17] = seiBufferSize >> 8;
	/* NOTE: vlc doesn't like '0' at byte 18 */
	seidata[18] = 0x1; //version
	seidata[19] = 0x1; //version
	memcpy(seidata + 20, source.constPars()->metaData.constData(), seiBufferSize - 20);
	if ((mVecs == MV_SEI || mVecs == MV_BOTH)) {
		/*
		 * There are limits on the MV insertion,
		 *
		 *	1.	Putting it on every frame is not possible,
		 *		bitrate will be too high
		 *	2.	One MV doesn't fit on one SEI frame, there is
		 *		an undocumented limit on encoders maximum seidata
		 *		size. So we need to split MVs into 2(at least)
		 *
		 *	So we send MVs at every 30 frame, first one containing
		 *	half of the MV buffer and the next frame containing
		 *	the remaining.
		 */
		int fcnt = getOutputQueue(0)->getSentCount() % 30;
		if (fcnt == 0) /* open space for mv on next frame */
			seiBufferSize += motVectSize / 2;
		else if (fcnt == 1) { /* write half-of-mv to this frame */
			uchar *motVectBuf = (uchar *)Buffer_getUserPtr(metadataBuf[1]);
			memcpy(seidata + 20 + seiBufferSize - 20, motVectBuf, motVectSize / 2);
			seiBufferSize += motVectSize / 2;
		} else if (fcnt == 2) { /* write other half-of-mv to this frame */
			uchar *motVectBuf = (uchar *)Buffer_getUserPtr(metadataBuf[1]);
			memcpy(seidata + 20 + seiBufferSize - 20, motVectBuf + motVectSize / 2, motVectSize / 2);
		}
	}
	return 0;
}

int H264Encoder::startCodec(bool alloc)
{
	mDebug("starting codec, parameters:\n\tMax frame rate: %d\n\t"
		   "Rate control: %d\n\t"
		   "Target bitrate: %d\n\t"
		   "Intra frame interval: %d\n\t"
		   "SEI enabled: %d\n\t"
		   , maxFrameRate, rateControl, videoBitRate, intraFrameInterval, seiEnabled);
	generateIdrFrame = false;
	timeStamp = 0;
	vuiparambuf = new VUIParamBuffer;
	*vuiparambuf = H264VENC_TI_VUIPARAMBUFFER;
	BufferGfx_Attrs         gfxAttrs            = BufferGfx_Attrs_DEFAULT;
	IH264VENC_Params         *params = new IH264VENC_Params;

	/* DM365 only supports YUV420P semi planar chroma format */
	ColorSpace_Type         colorSpace = ColorSpace_YUV420PSEMI;
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
	params->videncParams = Venc1_Params_DEFAULT;
	params->videncParams.size = sizeof(IH264VENC_Params);

	/*
	 * Set up codec parameters. We round up the height to accomodate for
	 * alignment restrictions from codecs
	 */
	params->videncParams.maxWidth = imageWidth;
	params->videncParams.maxHeight = Dmai_roundUp(imageHeight, CODECHEIGHTALIGN);
	if (colorSpace ==  ColorSpace_YUV420PSEMI) {
		params->videncParams.inputChromaFormat = XDM_YUV_420SP;
	} else {
		params->videncParams.inputChromaFormat = XDM_YUV_422ILE;
	}
	params->videncParams.reconChromaFormat = XDM_YUV_420SP;
	params->videncParams.maxFrameRate      = maxFrameRate;

	if (rateControl == RATE_CBR) {
		if (videoBitRate <= 0)
			videoBitRate = 4000000;
		params->videncParams.rateControlPreset = IVIDEO_LOW_DELAY;
		params->videncParams.maxBitRate = videoBitRate;
	} else if (rateControl == RATE_VBR) {
		params->videncParams.rateControlPreset = IVIDEO_STORAGE;
		params->videncParams.maxBitRate = 50000000;
		if (videoBitRate > 0)
			params->videncParams.maxBitRate = videoBitRate;
	} else if (rateControl == RATE_NONE) {
		params->videncParams.rateControlPreset = IVIDEO_NONE;
		params->videncParams.maxBitRate = 50000000;
	} else {
		params->videncParams.maxBitRate = videoBitRate;
		params->videncParams.rateControlPreset = IVIDEO_USER_DEFINED;
	}
	if (profileId == 0)
		setDefaultParams(params);
	else if (profileId == 1)
		setParamsProfile1(params);
	else if (profileId == 2)
		setParamsProfile2(params);
	else if (profileId == 3)
		setParamsProfile3(params);

	dynH264Params = new IH264VENC_DynamicParams;
	memset(dynH264Params, 0, sizeof(IH264VENC_DynamicParams));
	if (profileId == 0)
		setDefaultDynamicParams(params);
	else if (profileId == 1)
		setDynamicParamsProfile1(params);
	else if (profileId == 2)
		setDynamicParamsProfile2(params);
	else if (profileId == 3)
		setDynamicParamsProfile3(params);

	QString codecName;
	if (codec == CODEC_H264)
		codecName = "h264enc";
	else if (codec == CODEC_MPEG4)
		codecName = "mpeg4enc";
	else
		return -EINVAL;
	/* Create the video encoder */
	hCodec = Venc1_create(hEngine, (Char *)qPrintable(codecName), (VIDENC1_Params *)params,
						  (VIDENC1_DynamicParams *)dynH264Params);

	if (hCodec == NULL) {
		mDebug("Failed to create video encoder: %s", qPrintable(codecName));
		return -EINVAL;
	}

	/* Allocate output buffers */
	if (alloc) {
		gfxAttrs.colorSpace = colorSpace;
		gfxAttrs.dim.width  = imageWidth;
		gfxAttrs.dim.height = Dmai_roundUp(imageHeight, CODECHEIGHTALIGN);
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
		hBufTab = BufTab_create(numOutputBufs, bufSize,
								BufferGfx_getBufferAttrs(&gfxAttrs));

		if (hBufTab == NULL) {
			mDebug("Failed to allocate contiguous buffers");
			return -ENOMEM;
		}

		Buffer_Attrs attr = Buffer_Attrs_DEFAULT;
		frameinfoInterface = new FrameInfo_Interface;

		metadataBuf[0] = Buffer_create(sizeof(FrameInfo_Interface), &attr);

		if (mVecs != MV_NONE) {
			VIDENC1_Status status;
			status.size = sizeof(IH264VENC_Status);
			if (VIDENC1_control(Venc1_getVisaHandle(hCodec), XDM_GETBUFINFO,
								&dynH264Params->videncDynamicParams, &status) != VIDENC1_EOK) {
				qDebug("error setting control on encoder: 0x%x", (int)status.extendedError);
				printErrorMsg(status.extendedError);
			}
			metadataBuf[1] = Buffer_create(status.bufInfo.minOutBufSize[1], &attr);
			motVectSize = status.bufInfo.minOutBufSize[1];
		}

	}

	return 0;
}

int H264Encoder::stopCodec()
{
	return DmaiEncoder::stopCodec();
}

int H264Encoder::setDefaultParams(IH264VENC_Params *params)
{
	/*
	 * According to codec user guide, high speed or high quality
	 * setting does not make any difference on bitstream/performance
	 */
	params->videncParams.encodingPreset  = XDM_HIGH_SPEED;
	/* extended H.264 parameters */
	params->profileIdc = 100;
	params->levelIdc = 40;
	params->Log2MaxFrameNumMinus4 = 0;
	params->ConstraintSetFlag = 0;
	params->entropyMode = 0; //0: CAVLC, 1:CABAC
	params->transform8x8FlagIntraFrame = 0;
	params->transform8x8FlagInterFrame = 0;
	params->enableVUIparams = enableVUIParams;
	params->meAlgo = 0;
	if (mVecs != MV_NONE)
		params->meAlgo = 1;
	params->seqScalingFlag = 0;
	params->encQuality = 2;
	params->enableARM926Tcm = 0;
	params->enableDDRbuff = 0;
    params->sliceMode = 0;
	params->numTemporalLayers = 0;
	params->svcSyntaxEnable = 0;
	params->EnableLongTermFrame = 0;
	params->outputDataMode = 1;
	params->sliceFormat = 1;

	return 0;
}

int H264Encoder::setParamsProfile1(IH264VENC_Params *params)
{
	setDefaultParams(params);
	//params->videncParams.encodingPreset = XDM_HIGH_QUALITY;
	params->levelIdc = 50;
	params->entropyMode = 1; //CABAC
	params->transform8x8FlagIntraFrame = 1;
	params->transform8x8FlagInterFrame = 0;
	params->meAlgo = 1;
	params->seqScalingFlag = 1;
	params->encQuality = 2;

	return 0;
}

int H264Encoder::setParamsProfile2(IH264VENC_Params *params)
{
	setParamsProfile1(params);
	params->videncParams.encodingPreset = XDM_USER_DEFINED;
	/* standard = 0, high = 1, lite = 2 */
	params->encQuality = 0;
	params->profileIdc = 100;
	params->transform8x8FlagInterFrame = 0;

	return 0;
}

int H264Encoder::setParamsProfile3(IH264VENC_Params *params)
{
	setParamsProfile1(params);
	params->videncParams.encodingPreset = XDM_USER_DEFINED;
	/* standard = 0, high = 1, lite = 2 */
	params->encQuality = 0;
	params->profileIdc = 100;
	params->transform8x8FlagInterFrame = 0;

	return 0;
}

int H264Encoder::setDefaultDynamicParams(IH264VENC_Params *params)
{
	dynH264Params->videncDynamicParams = Venc1_DynamicParams_DEFAULT;
	dynH264Params->VUI_Buffer = vuiparambuf;
	dynH264Params->VUI_Buffer->numUnitsInTicks = 100;
	dynH264Params->VUI_Buffer->timeScale = 100 * encodeFps * 2;
	dynH264Params->videncDynamicParams.size = sizeof(IH264VENC_DynamicParams);
	dynH264Params->videncDynamicParams.targetBitRate   = params->videncParams.maxBitRate;
	dynH264Params->videncDynamicParams.inputWidth      = imageWidth;
	dynH264Params->videncDynamicParams.inputHeight     = imageHeight;
	dynH264Params->videncDynamicParams.refFrameRate    = params->videncParams.maxFrameRate;
	dynH264Params->videncDynamicParams.targetFrameRate = params->videncParams.maxFrameRate;
	dynH264Params->videncDynamicParams.interFrameInterval = 0;
	dynH264Params->videncDynamicParams.intraFrameInterval = intraFrameInterval;
	dynH264Params->sliceSize = 0;
	dynH264Params->airRate = 0;
	dynH264Params->interPFrameQP = 28;
	dynH264Params->intraFrameQP = 28;
	dynH264Params->initQ = -1;
	dynH264Params->rcQMax = 51;
	dynH264Params->rcQMin = 0;
	dynH264Params->rcQMaxI = 51;
	dynH264Params->rcQMinI = 0;
	dynH264Params->rcAlgo = 1;
	dynH264Params->maxDelay = 2000; //2 secs, default value
	dynH264Params->lfDisableIdc = 0;
	dynH264Params->enableBufSEI = enableBufSei;
	dynH264Params->enablePicTimSEI = enablePicTimSei;
	dynH264Params->perceptualRC = 0;
	dynH264Params->mvSADoutFlag = 0;
	if (mVecs != MV_NONE)
		dynH264Params->mvSADoutFlag = 1;
	dynH264Params->resetHDVICPeveryFrame = 1;
	dynH264Params->enableROI = 0;
	dynH264Params->metaDataGenerateConsume = 0;
	dynH264Params->maxBitrateCVBR = 768000;
	dynH264Params->interlaceRefMode = 0;
	dynH264Params->enableGDR = 0;
	dynH264Params->GDRduration = 5;
	dynH264Params->GDRinterval = 30;
	dynH264Params->LongTermRefreshInterval = 0;
	dynH264Params->UseLongTermFrame = 0;
	dynH264Params->SetLongTermFrame = 0;
	dynH264Params->CVBRsensitivity = 0;
	dynH264Params->CVBRminbitrate = 0;
	dynH264Params->LBRmaxpicsize = 0;
	dynH264Params->LBRminpicsize = 0;
	dynH264Params->LBRskipcontrol = 0;
	dynH264Params->maxHighCmpxIntCVBR = 0;
	dynH264Params->disableMVDCostFactor = 0;
	dynH264Params->aspectRatioX = 1;
	dynH264Params->aspectRatioY = 1;
	/*
	 * NOTE: According to codec datasheet making
	 * idrFrameInterval '1' makes all I frames
	 * IDR frames which is not TRUE. Making it
	 * '1' makes *ALL* frames IDR. Correct way of
	 * making all I frames IDR is to set this parameter
	 * to intraFrameInterval. Also note that there is
	 * no any noticable bandwidth penalty using IDR frames
	 * so we make all I IDR.
	 */
	dynH264Params->idrFrameInterval = intraFrameInterval; //no I frames, all will be IDR

	/* roi */
	if (!roiRect.isNull())
		dynH264Params->enableROI = 1;

	/* multi-pass encoding */
	if (genMetadata)
		dynH264Params->metaDataGenerateConsume = 1;
	else if (useMetadata)
		dynH264Params->metaDataGenerateConsume = 2;

	return 0;
}

int H264Encoder::setDynamicParamsProfile1(IH264VENC_Params *params)
{
	setDefaultDynamicParams(params);
	dynH264Params->initQ = -1;
	dynH264Params->rcQMax = 44;

	return 0;
}

int H264Encoder::setDynamicParamsProfile2(IH264VENC_Params *params)
{
	setDefaultDynamicParams(params);
	dynH264Params->initQ = -1;
	dynH264Params->rcQMax = 44;
	dynH264Params->rcQMin = 8;
	dynH264Params->rcAlgo = 0;
	dynH264Params->perceptualRC = 1;

	return 0;
}

int H264Encoder::setDynamicParamsProfile3(IH264VENC_Params *params)
{
	setDefaultDynamicParams(params);
	dynH264Params->initQ = -1;
	dynH264Params->rcQMax = 44;
	dynH264Params->rcQMaxI = 40;
	dynH264Params->rcQMin = 8;
	dynH264Params->rcQMinI = 8;
	dynH264Params->rcAlgo = 0;
	dynH264Params->perceptualRC = 1;
	dynH264Params->maxDelay = 300;

	return 0;
}

ROI_Interface * H264Encoder::roiParameter(uchar *vdata)
{
	if (dynH264Params->enableROI) {
		ROI_Interface *roi = new ROI_Interface;
		int x = roiRect.left(), y = roiRect.top(), w = roiRect.width(), h = roiRect.height();
		roi->listROI[0].topLeft.x = x;
		roi->listROI[0].topLeft.y = y;
		roi->listROI[0].bottomRight.x = x + w;
		roi->listROI[0].bottomRight.y = y + h;
		roi->numOfROI = 1;
		roi->roiPriority[0] = 4;
		roi->roiType[0] = FOREGROUND_OBJECT;
		if (vdata && markRoi) {
			QElapsedTimer t; t.start();
			int linewidth = 1;
			int col = 255;
			/* top line */
			for (int i = y; i < y + linewidth; i++)
				memset(&vdata[i * 1280] + x, col, w);
			/* bottom line */
			for (int i = y + h - linewidth; i < y + h; i++)
				memset(&vdata[i * 1280] + x, col, w);
			/* left line */
			for (int i = y; i < y + h; i++)
				memset(&vdata[i * 1280] + x, col, linewidth);
			/* right line */
			for (int i = y; i < y + h; i++)
				memset(&vdata[i * 1280] + x + w - linewidth, col, linewidth);
			mInfo("roi mark took %lld msecs", t.elapsed());
		}
		return roi;
	}
	return NULL;
}
