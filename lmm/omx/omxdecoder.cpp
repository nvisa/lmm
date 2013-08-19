#include "omxdecoder.h"

#include <lmm/debug.h>

#include <errno.h>

#include <xdc/std.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Types.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Core.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Video.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Component.h>
#include <ti/omx/interfaces/openMaxv11/OMX_TI_Common.h>
#include <ti/omx/interfaces/openMaxv11/OMX_TI_Index.h>
#include <ti/omx/interfaces/openMaxv11/OMX_TI_Video.h>

#define align(a,b)  ((((uint32_t)(a)) + (b)-1) & (~((uint32_t)((b)-1))))
#define OMX_VIDDEC_INPUT_PORT  0
#define OMX_VIDDEC_OUTPUT_PORT 1
#define INPUT_BUFFER_COUNT 4
#define OUTPUT_BUFFER_COUNT 6

OmxDecoder::OmxDecoder(QObject *parent) :
	BaseOmxElement(parent)
{
	codingType = OMX_VIDEO_CodingAVC;
	targetFps = 60;
	width = 1920;
	height = 1080;
}

int OmxDecoder::setFrameSize(QSize sz)
{
	width = sz.width();
	height = sz.height();
	return 0;
}

int OmxDecoder::processBuffer(RawBuffer buf)
{
	bufLock.lock();
	OMX_BUFFERHEADERTYPE *omxBuf = NULL;
	if (availBuffers.size()) {
		omxBuf = availBuffers.takeFirst();
		busyBuffers << omxBuf;
	}
	if (!omxBuf) {
		mLog("no omx buf, waiting...");
		wci.wait(&bufLock);
		mLog("woke-up");
		if (availBuffers.size()) {
			omxBuf = availBuffers.takeFirst();
			busyBuffers << omxBuf;
		}
	}
	bufLock.unlock();
	if (!omxBuf) {
		mDebug("cannot get a buffer");
		return -ENOENT;
	}
	handleLock.lock();
	if (!handleDec) {
		handleLock.unlock();
		return -EINVAL;
	}
	memcpy(omxBuf->pBuffer, buf.constData(), buf.size());
	omxBuf->nFilledLen = buf.size();
	omxBuf->nOffset = 0;
	omxBuf->nAllocLen = buf.size();
	omxBuf->nInputPortIndex = 0;
	OMX_ERRORTYPE err = OMX_EmptyThisBuffer(handleDec, omxBuf);
	handleLock.unlock();
	if (err != OMX_ErrorNone) {
		mDebug("error passing buffer to omx component: %s", omxError(err));
		return -EINVAL;
	}
	mInfo("new data with size %d passed to omx component", buf.size());

	return 0;
}

const QList<QPair<OMX_BUFFERHEADERTYPE *, int> > OmxDecoder::getDecoderBuffers()
{
	QList<QPair<OMX_BUFFERHEADERTYPE *, int> > list;
	for (int i = 0; i < OUTPUT_BUFFER_COUNT; i++) {
		list << QPair<OMX_BUFFERHEADERTYPE *, int>(decOutBufs[i], outputBufferSize);
	}
	return list;
}

int OmxDecoder::startComponent()
{
	int reterr = 0;
	handleDec = NULL;
	OMX_ERRORTYPE err = OMX_GetHandle(&handleDec, (OMX_STRING) "OMX.TI.DUCATI.VIDDEC"
									  , this, omxCallbacks);
	mInfo("decoder compoenent is created");
	if ((err != OMX_ErrorNone) || (handleDec == NULL)) {
		mDebug("Error in Get Handle function : %s", omxError(err));
		reterr = -ENOENT;
		goto error_alloc_dechandle;
	}
	err = (OMX_ERRORTYPE)setOmxDecoderParams();
	if (err != OMX_ErrorNone) {
		mDebug("Error in setOmxDecoderParams() function : %s", omxError(err));
		reterr = -EINVAL;
		goto error_alloc_dec_setparams;
	}
	mDebug("decoder params set successfully");

	err = OMX_SendCommand(handleDec, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting decoder state to idle: %s", omxError(err));
		reterr = -EINVAL;
		goto error_dec_cmd_idle;
	}
	mDebug("decoder state set to idle");

	/* Allocate decoder buffers */
	for (int i = 0; i < INPUT_BUFFER_COUNT; i++) {
		mDebug("allocating input buffer %d", i);
		err = OMX_AllocateBuffer(handleDec, &decInBufs[i],
								 OMX_VIDDEC_INPUT_PORT, this,
								 width * height); //TODO: get value from videoutils
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_AllocateBuffer()- Input Port State set : %s",
				   omxError(err));
			reterr = -ENOMEM;
			goto error_alloc_input_buffer;
		}
	}
	mDebug("input buffers allocated");
	/*
	 * decoder outputs always YUV420 semi-planar, needs 24 bytes padding on y and
	 * 32 bytes padding on x. X values should aligned on 128 bytes boundry
	 *
	 * TODO: get value from videoutils
	 */
	for (int i = 0; i < OUTPUT_BUFFER_COUNT; i++) {
		mDebug("allocating output buffer %d with size %d", i, outputBufferSize);
		err = OMX_AllocateBuffer(handleDec, &decOutBufs[i], OMX_VIDDEC_OUTPUT_PORT, this, outputBufferSize);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_AllocateBuffer()- Output Port State set : %s",
				   omxError(err));
			reterr = -ENOMEM;
			goto error_alloc_output_buffer;
		}
	}
	if (waitOmxComp()) {
		mDebug("error waiting for component to go idle state");
		reterr = -EBUSY;
		goto error_alloc_dechandle; //TODO: Correct this
	}
	mDebug("decoder output buffers allocated");

	/* All input buffers are free(owned by us) at the beginning */
	for (int i = 0; i < INPUT_BUFFER_COUNT; i++)
		availBuffers << decInBufs[i];

	if (execute())
		goto error_giving_output_buffers;

	mDebug("omx decoder setted-up sucessfully");
	return 0;

error_giving_output_buffers:
error_alloc_dec_setparams:
	mDebug("freeing decoder output buffers");
	for (int i = 0; i < OUTPUT_BUFFER_COUNT; i++) {
		err = OMX_FreeBuffer(handleDec, OMX_VIDDEC_OUTPUT_PORT, decOutBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_FreeBuffer(): %s", omxError(err));
		}
	}
error_alloc_output_buffer:
	mDebug("freeing decoder input buffers");
	for (int i = 0; i < INPUT_BUFFER_COUNT; i++) {
		err = OMX_FreeBuffer(handleDec, OMX_VIDDEC_INPUT_PORT, decInBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_FreeBuffer(): %s", omxError(err));
		}
	}
error_alloc_input_buffer:
error_dec_cmd_idle:
	mDebug("freeing decoder handle");
	err = OMX_FreeHandle(handleDec);
	if (err != OMX_ErrorNone)
		mDebug("error freeing decoder handle: %s", omxError(err));
error_alloc_dechandle:
	mDebug("!!! error %d !!!", reterr);
	return reterr;
}

int OmxDecoder::stopComponent()
{
	handleLock.lock();
	OMX_ERRORTYPE err = OMX_SendCommand(handleDec, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting decoder state to idle: %s", omxError(err));
	} else
		waitOmxComp();

	err = OMX_SendCommand(handleDec, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting decoder state to loaded: %s", omxError(err));
	}

	mDebug("freeing decoder buffers");
	for (int i = 0; i < INPUT_BUFFER_COUNT; i++) {
		err = OMX_FreeBuffer(handleDec, OMX_VIDDEC_INPUT_PORT, decInBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_FreeBuffer(): %s", omxError(err));
		}
	}
	for (int i = 0; i < OUTPUT_BUFFER_COUNT; i++) {
		err = OMX_FreeBuffer(handleDec, OMX_VIDDEC_OUTPUT_PORT, decOutBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_FreeBuffer(): %s", omxError(err));
		}
	}
	waitOmxComp();

	mDebug("freeing decoder handle");
	err = OMX_FreeHandle(handleDec);
	if (err != OMX_ErrorNone)
		mDebug("error freeing decoder handle: %s", omxError(err));
	handleDec = NULL;
	handleLock.unlock();
	bufLock.lock();
	availBuffers.clear();
	wci.wakeAll();
	bufLock.unlock();
	mDebug("decoder cleaned-up");
	return 0;
}

#define OMX_INIT_PARAM(param)                                                  \
{                                                                      \
	memset ((param), 0, sizeof (*(param)));                              \
	(param)->nSize = sizeof (*(param));                                  \
	(param)->nVersion.s.nVersionMajor = 1;                               \
	(param)->nVersion.s.nVersionMinor = 1;                               \
	}
int OmxDecoder::setOmxDecoderParams()
{
	OMX_U8 PADX = 32;
	OMX_U8 PADY = 24;

	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_HANDLETYPE pHandle = handleDec;
	OMX_PORT_PARAM_TYPE portInit;
	OMX_PARAM_PORTDEFINITIONTYPE pInPortDef, pOutPortDef;
	OMX_VIDEO_PARAM_STATICPARAMS tStaticParam;
	OMX_PARAM_COMPPORT_NOTIFYTYPE pNotifyType;

	if (!pHandle)
	{
		eError = OMX_ErrorBadParameter;
		goto EXIT;
	}
	mDebug("PADX: %d PADY: %d",PADX, PADY);

	OMX_INIT_PARAM (&portInit);

	portInit.nPorts = 2;
	portInit.nStartPortNumber = 0;
	eError = OMX_SetParameter (pHandle, OMX_IndexParamVideoInit, &portInit);
	if (eError != OMX_ErrorNone)
	{
		goto EXIT;
	}

	/* Set the component's OMX_PARAM_PORTDEFINITIONTYPE structure (input) */

	OMX_INIT_PARAM (&pInPortDef);

	/* populate the input port definataion structure, It is Standard OpenMax
		 structure */
	/* set the port index */
	pInPortDef.nPortIndex = OMX_VIDDEC_INPUT_PORT;
	/* It is input port so direction is set as Input, Empty buffers call would be
		 accepted based on this */
	pInPortDef.eDir = OMX_DirInput;
	/* number of buffers are set here */
	pInPortDef.nBufferCountActual = INPUT_BUFFER_COUNT;
	/* buffer size by deafult is assumed as width * height for input bitstream
		 which would suffice most of the cases */
	pInPortDef.nBufferSize = width * height;

	pInPortDef.bEnabled = OMX_TRUE;
	pInPortDef.bPopulated = OMX_FALSE;

	/* OMX_VIDEO_PORTDEFINITION values for input port */
	/* set the width and height, used for buffer size calculation */
	pInPortDef.format.video.nFrameWidth = width;
	pInPortDef.format.video.nFrameHeight = height;
	/* for bitstream buffer stride is not a valid parameter */
	pInPortDef.format.video.nStride = -1;
	/* component supports only frame based processing */

	/* bitrate does not matter for decoder */
	pInPortDef.format.video.nBitrate = 104857600;
	/* as per openmax frame rate is in Q16 format */
	pInPortDef.format.video.xFramerate = (targetFps) << 16;
	/* input port would receive encoded stream */
	pInPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
	/* this is codec setting, OMX component does not support it */
	/* color format is irrelavant */
	pInPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;

	eError =
			OMX_SetParameter (pHandle, OMX_IndexParamPortDefinition, &pInPortDef);
	if (eError != OMX_ErrorNone)
	{
		goto EXIT;
	}

	/* Set the component's OMX_PARAM_PORTDEFINITIONTYPE structure (output) */
	OMX_INIT_PARAM (&pOutPortDef);

	/* setting the port index for output port, properties are set based on this
		 index */
	pOutPortDef.nPortIndex = OMX_VIDDEC_OUTPUT_PORT;
	pOutPortDef.eDir = OMX_DirOutput;
	/* componet would expect these numbers of buffers to be allocated */
	pOutPortDef.nBufferCountActual = OUTPUT_BUFFER_COUNT;

	/*
	 * codec requires padded height and width and width needs to be aligned
	 * at 128 byte boundary. For 1920x1080:
	 *		1184 * 3 * 2048 / 2 = 3,637,248
	 */
	pOutPortDef.nBufferSize =
			((((width + (2 * PADX) +
				127) & 0xFFFFFF80) * ((((height + 15) & 0xfffffff0) + (4 * PADY))) * 3) >> 1);
	outputBufferSize = pOutPortDef.nBufferSize;
	pOutPortDef.bEnabled = OMX_TRUE;
	pOutPortDef.bPopulated = OMX_FALSE;
	/* currently component alloactes contigous buffers with 128 alignment, these
		 values are do't care */

	/* OMX_VIDEO_PORTDEFINITION values for output port */
	pOutPortDef.format.video.nFrameWidth = width;
	pOutPortDef.format.video.nFrameHeight = ((height + 15) & 0xfffffff0);
	/* stride is set as buffer width */
	pOutPortDef.format.video.nStride =
			align ((width + (2 * PADX)), 128);
	//pOutPortDef.format.video.cMIMEType = "H264";
	pOutPortDef.format.video.pNativeRender = NULL;

	/* bitrate does not matter for decoder */
	pOutPortDef.format.video.nBitrate = 25000000;
	/* as per openmax frame rate is in Q16 format */
	pOutPortDef.format.video.xFramerate = (targetFps) << 16;
	/* output is raw YUV 420 SP format, It support only this */
	pOutPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
	pOutPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;

	eError =
			OMX_SetParameter (pHandle, OMX_IndexParamPortDefinition, &pOutPortDef);
	if (eError != OMX_ErrorNone)
	{
		eError = OMX_ErrorBadParameter;
		goto EXIT;
	}

	eError =
			OMX_GetParameter (pHandle, OMX_IndexParamPortDefinition, &pOutPortDef);
	if (eError != OMX_ErrorNone)
	{
		eError = OMX_ErrorBadParameter;
		goto EXIT;
	}
	videoStride = pOutPortDef.format.video.nStride;
	//pAppData->nDecStride = pOutPortDef.format.video.nStride;

	/* Make VDEC execute periodically based on fps */
	OMX_INIT_PARAM(&pNotifyType);
	pNotifyType.eNotifyType = OMX_NOTIFY_TYPE_NONE;
	pNotifyType.nPortIndex =  OMX_VIDDEC_INPUT_PORT;
	eError =
			OMX_SetParameter (pHandle, (OMX_INDEXTYPE)OMX_TI_IndexParamCompPortNotifyType,
							  &pNotifyType);
	if (eError != OMX_ErrorNone)
	{
		mDebug("input port OMX_SetParameter failed");
		eError = OMX_ErrorBadParameter;
		goto EXIT;
	}
	pNotifyType.eNotifyType = OMX_NOTIFY_TYPE_NONE;
	pNotifyType.nPortIndex =  OMX_VIDDEC_OUTPUT_PORT;
	eError =
			OMX_SetParameter (pHandle, (OMX_INDEXTYPE)OMX_TI_IndexParamCompPortNotifyType,
							  &pNotifyType);
	if (eError != OMX_ErrorNone)
	{
		mDebug("output port OMX_SetParameter failed");
		eError = OMX_ErrorBadParameter;
		goto EXIT;
	}

	/* Set the codec's static parameters, set it at the end, so above parameter settings
		 is not disturbed */
	if (codingType == OMX_VIDEO_CodingAVC) {

		OMX_INIT_PARAM (&tStaticParam);

		tStaticParam.nPortIndex = OMX_VIDDEC_OUTPUT_PORT;

		eError = OMX_GetParameter (pHandle, (OMX_INDEXTYPE)OMX_TI_IndexParamVideoStaticParams,
								   &tStaticParam);
		/* setting I frame interval */
		mDebug( " level set is %d", (int) tStaticParam.videoStaticParams.h264DecStaticParams.presetLevelIdc);

		tStaticParam.videoStaticParams.h264DecStaticParams.presetLevelIdc = IH264VDEC_LEVEL42;

		eError = OMX_SetParameter (pHandle, (OMX_INDEXTYPE)OMX_TI_IndexParamVideoStaticParams,
								   &tStaticParam);
	}
	return 0;
EXIT:
	return eError;
}

int OmxDecoder::execute()
{
	OMX_ERRORTYPE err = OMX_SendCommand(handleDec, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting decoder state to executing: %s", omxError(err));
		return -EINVAL;
	}
	if (waitOmxComp()) {
		mDebug("error waiting for component to go executing state");
		return -EBUSY;
	}

	/* All output buffers should be busy(owned by codec) at the beginning */
	for (int i = 0; i < OUTPUT_BUFFER_COUNT; i++) {
		busyOutBuffers << decOutBufs[i];
		err = OMX_FillThisBuffer(handleDec, decOutBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("error giving output buffer %d to decoder: %s", i, omxError(err));
			return -EINVAL;
		}
	}

	mDebug("omx decoder executed");
	return 0;
}

void OmxDecoder::handleDispBuffer(OMX_BUFFERHEADERTYPE *omxBuf)
{
	RawBuffer buf(this);
	buf.setRefData("video/x-raw-int", omxBuf->pBuffer, omxBuf->nFilledLen);
	buf.addBufferParameter("omxBuf", (int)omxBuf);
	mLog("filled=%u size=%u alloc=%u offset=%u flags=0x%x input=%u output=%u",
		   (uint32_t)omxBuf->nFilledLen, (uint32_t)omxBuf->nSize, (uint32_t)omxBuf->nAllocLen,
		   (uint32_t)omxBuf->nOffset, (uint32_t)omxBuf->nFlags, (uint32_t)omxBuf->nInputPortIndex,
		   (uint32_t)omxBuf->nOutputPortIndex);
	bufLock.lock();
	if (busyOutBuffers.contains(omxBuf)) {
		availOutBuffers << omxBuf;
		busyOutBuffers.removeOne(omxBuf);
	}
	bufLock.unlock();

	newOutputBuffer(0, buf);
}

void OmxDecoder::handleInputBufferDone(OMX_BUFFERHEADERTYPE *omxBuf)
{
	mInfo("releasing input buffer %p", omxBuf);
	bufLock.lock();
	availBuffers << omxBuf;
	busyBuffers.removeOne(omxBuf);
	wci.wakeAll();
	bufLock.unlock();
}
