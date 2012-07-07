#include "h264encoder.h"
#include "dmaiencoder.h"
#include "rawbuffer.h"
#include "emdesk/debug.h"
#include "dm365/ih264venc.h"
#include "streamtime.h"
#include "cpuload.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/video1/videnc1.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/ColorSpace.h>
#include <ti/sdo/dmai/ce/Venc1.h>
#include <ti/sdo/dmai/BufferGfx.h>

#include <errno.h>

#include <QDateTime>

#define VIDEO_PIPE_SIZE 4
#define CODECHEIGHTALIGN 16

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
	maxFrameRate = 30000;
	rateControl = RATE_NONE;
	videoBitRate = -1;
	intraFrameInterval = 30;
	seiBufferSize = 0;
}

int H264Encoder::flush()
{
	if (codec == CODEC_H264 && encodeCount) {
		mDebug("flusing encoder, generating IDR frame");
		generateIdrFrame = true;
	}
	return DmaiEncoder::flush();
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
	VIDENC1_DynamicParams   dynParams;
} Venc1_Object;
/* As 0 is not a valid buffer id in XDM 1.0, we need macros for easy access */
#define GETID(x)  ((x) + 1)
#define GETIDX(x) ((x) - 1)

static Int Venc1_processL(Venc1_Handle hVe, Buffer_Handle hInBuf, Buffer_Handle hOutBuf, int seiDataSize, int *seiDataOffset)
{
	IVIDEO1_BufDescIn       inBufDesc;
	XDM_BufDesc             outBufDesc;
	XDAS_Int32              outBufSizeArray[1];
	XDAS_Int32              status;
	IH264VENC_InArgs		inArgs;
	IH264VENC_OutArgs       outArgs;
	XDAS_Int8              *inPtr;
	XDAS_Int8              *outPtr;
	BufferGfx_Dimensions    dim;
	Int32                  offset = 0;
	Uint32                  bpp;

	bpp = ColorSpace_getBpp(BufferGfx_getColorSpace(hInBuf));

	BufferGfx_getDimensions(hInBuf, &dim);

	offset = (dim.y * dim.lineLength) + (dim.x * (bpp >> 3));
	assert(offset < Buffer_getSize(hInBuf));

	inPtr  = Buffer_getUserPtr(hInBuf)  + offset;
	outPtr = Buffer_getUserPtr(hOutBuf);

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
	outBufDesc.bufs                     = &outPtr;
	outBufDesc.bufSizes                 = outBufSizeArray;

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
	inArgs.timeStamp = 0;
	inArgs.roiParameters.numOfROI = 0;

	outArgs.videncOutArgs.size                        = sizeof(IH264VENC_OutArgs);

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
		VIDENC1_Status status;
		status.size = sizeof(IH264VENC_Status);
		VIDENC1_DynamicParams  *dynParams = &defaultDynParams;
		dynParams->forceFrame = IVIDEO_IDR_FRAME;
		if (VIDENC1_control(Venc1_getVisaHandle(hCodec), XDM_SETPARAMS,
							dynParams, &status) != VIDENC1_EOK) {
			qDebug("error setting control on encoder: 0x%x", (int)status.extendedError);
			printErrorMsg(status.extendedError);
		} else
			idrGenerated = true;
	}

	mInfo("invoking venc1_process: width=%d height=%d",
		  (int)dim.width, (int)dim.height);
	/* Encode the video buffer */
	int seiDataOffset;
	if (Venc1_processL(hCodec, buffer, hDstBuf, seiBufferSize, &seiDataOffset) < 0) {
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
		status.size = sizeof(IH264VENC_Status);
		VIDENC1_DynamicParams  *dynParams = &defaultDynParams;
		dynParams->forceFrame = IVIDEO_NA_FRAME;
		if (VIDENC1_control(Venc1_getVisaHandle(hCodec), XDM_SETPARAMS,
							dynParams, &status) != VIDENC1_EOK)
			qDebug("error setting control on encoder: 0x%x", (int)status.extendedError);
		generateIdrFrame = false;
	}
	mInfo("inserting %d bytes sei user data at offset %d, total size is %d",
		   seiBufferSize, seiDataOffset, (int)Buffer_getNumBytesUsed(hDstBuf));
	/* insert SEI data */
	if (seiDataOffset) {
		char *seidata = (char *)Buffer_getUserPtr(hDstBuf) + seiDataOffset;
		/* add IEC 11578 uuid */
		for (int i = 0; i < 16; i++)
			seidata[i] = 0xAA;
		seidata[16] = seiBufferSize & 0xff;
		seidata[17] = seiBufferSize >> 16;
		/* NOTE: vlc doesn't like '0' at byte 18 */
		seidata[18] = 0x1; //version
		QByteArray ba(seidata + 19, seiBufferSize - 19);
		addSeiData(&ba, source);
		memcpy(seidata + 19, ba.constData(), ba.size());
		//for (int i = 19; i < seiBufferSize; i++)
			//seidata[i] = i - 19;
	}
	RawBuffer buf = RawBuffer(this);
	buf.setRefData(Buffer_getUserPtr(hDstBuf), Buffer_getNumBytesUsed(hDstBuf));
	buf.addBufferParameter("dmaiBuffer", (int)hDstBuf);
	buf.addBufferParameter("frameType", (int)BufferGfx_getFrameType(buffer));
	buf.addBufferParameter("fps", 30); //TODO: generalize fps
	Buffer_setUseMask(hDstBuf, Buffer_getUseMask(hDstBuf) | 0x1);
	buf.setStreamBufferNo(encodeCount++);
	/* Reset the dimensions to what they were originally */
	BufferGfx_resetDimensions(buffer);
	outputLock.lock();
	outputBuffers << buf;
	outputLock.unlock();

	return 0;
}

int H264Encoder::startCodec()
{
	generateIdrFrame = false;
	defaultDynParams = Venc1_DynamicParams_DEFAULT;
	VIDENC1_Params          defaultParams       = Venc1_Params_DEFAULT;
	BufferGfx_Attrs         gfxAttrs            = BufferGfx_Attrs_DEFAULT;
	VIDENC1_Params         *params;
	VIDENC1_DynamicParams  *dynParams;

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
	dynParams = &defaultDynParams;

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
		params->maxBitRate = 50000000;
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
		hBufTab = BufTab_create(VIDEO_PIPE_SIZE, bufSize,
								BufferGfx_getBufferAttrs(&gfxAttrs));

		if (hBufTab == NULL) {
			mDebug("Failed to allocate contiguous buffers");
			return -ENOMEM;
		}

	}

	return 0;
}

int H264Encoder::stopCodec()
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

void H264Encoder::addSeiData(QByteArray *ba, const RawBuffer source)
{
	QDataStream out(ba, QIODevice::WriteOnly);
	out.setByteOrder(QDataStream::LittleEndian);
	out << (qint32)0x11223344; //version
	out << (qint64)streamTime->getCurrentTime();
	out << (qint64)source.getBufferParameter("captureTime").toLongLong();
	out << (qint32)QDateTime::currentDateTime().toTime_t();
	out << (qint32)CpuLoad::getCpuLoad();
	out << (qint32)CpuLoad::getAverageCpuLoad();
	out << (qint32)sentBufferCount;
}
