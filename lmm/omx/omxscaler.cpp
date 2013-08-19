#include "omxscaler.h"

#include <lmm/debug.h>

#include <xdc/std.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Types.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Core.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Video.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Component.h>
#include <ti/omx/interfaces/openMaxv11/OMX_TI_Common.h>
#include <ti/omx/interfaces/openMaxv11/OMX_TI_Index.h>
#include <ti/omx/interfaces/openMaxv11/OMX_TI_Video.h>
#include <ti/omx/comp/vfpc/omx_vfpc.h>

#include <errno.h>

#define OUTPUT_BUFFER_COUNT 6

OmxScaler::OmxScaler(QObject *parent) :
	BaseOmxElement(parent)
{
	handleScalar = NULL;
	width = 1920;
	height = 1080;
	outputWidth = 1920;
	outputHeight = 1080;
}

int OmxScaler::setInputFrameSize(QSize sz)
{
	width = sz.width();
	height = sz.height();
	return 0;
}

int OmxScaler::setOutputFrameSize(QSize sz)
{
	outputWidth = sz.width();
	outputHeight = sz.height();
	return 0;
}

int OmxScaler::execute()
{
	OMX_ERRORTYPE err = OMX_SendCommand(handleScalar, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting scalar state to executing: %s", omxError(err));
		return -EINVAL;
	}
	if (waitOmxComp()) {
		mDebug("error waiting for component to go executing state");
		return -EBUSY;
	}

	/* All output buffers should be busy(owned by scalar) at the beginning */
	for (int i = 0; i < OUTPUT_BUFFER_COUNT; i++) {
		busyOutBuffers << scOutBufs[i];
		err = OMX_FillThisBuffer(handleScalar, scOutBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("error giving output buffer %d to scalar: %s", i, omxError(err));
			return -EINVAL;
		}
	}
	mDebug("omx scalar executed");
	return 0;
}

int OmxScaler::processBuffer(RawBuffer buf)
{
	OMX_BUFFERHEADERTYPE *omxBuf = (OMX_BUFFERHEADERTYPE *)buf.getBufferParameter("omxBuf").toInt();
	if (!omxBuf) {
		mDebug("no omx buffers available");
		return -EINVAL;
	}
	OMX_BUFFERHEADERTYPE *mybuf = bufMap[omxBuf];
	mybuf->nFilledLen = omxBuf->nFilledLen;
	mybuf->nOffset = omxBuf->nOffset;
	mybuf->nTimeStamp = omxBuf->nTimeStamp;
	mybuf->nFlags = omxBuf->nFlags;
	mybuf->hMarkTargetComponent = omxBuf->hMarkTargetComponent;
	mybuf->pMarkData = omxBuf->pMarkData;
	mybuf->nTickCount = omxBuf->nTickCount;
	handleLock.lock();
	OMX_ERRORTYPE err = OMX_EmptyThisBuffer(handleScalar, mybuf);
	handleLock.unlock();
	if (err != OMX_ErrorNone) {
		mDebug("error passing buffer %p(%p) to omx component: %s", mybuf, omxBuf, omxError(err));
		return -EINVAL;
	}
	bufLock.lock();
	rawBufMap.insert(mybuf, buf);
	bufLock.unlock();

	return 0;
}

int OmxScaler::startComponent()
{
	int reterr = 0;
	OMX_ERRORTYPE  err = OMX_GetHandle(&handleScalar, (OMX_STRING)"OMX.TI.VPSSM3.VFPC.INDTXSCWB", this, omxCallbacks);
	if (err != OMX_ErrorNone || !handleScalar) {
		mDebug("error getting scalar component: %s", omxError(err));
		reterr = -ENOENT;
		goto error_alloc_schandle;
	}

	err = (OMX_ERRORTYPE)setOmxScalarParams();
	if (err != OMX_ErrorNone) {
		mDebug("Error in setOmxScalarParams() function : %s", omxError(err));
		reterr = -EINVAL;
		goto error_alloc_sc_setparams;
	}
	mDebug("scalar params set successfully");

	OMX_SendCommand(handleScalar, OMX_CommandPortEnable, OMX_VFPC_INPUT_PORT_START_INDEX, NULL);
	if (waitOmxComp()) {
		mDebug("error waiting for component to enable input port");
		reterr = -EBUSY;
		goto error_enable_port;
	}
	OMX_SendCommand(handleScalar, OMX_CommandPortEnable, OMX_VFPC_OUTPUT_PORT_START_INDEX, NULL);
	if (waitOmxComp()) {
		mDebug("error waiting for component to enable output port");
		reterr = -EBUSY;
		goto error_enable_port;
	}

	err = OMX_SendCommand(handleScalar, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting scalar state to idle: %s", omxError(err));
		reterr = -EINVAL;
		goto error_scalar_cmd_idle;
	}
	mDebug("scalar state set to idle");

	for (int i = 0; i < decoderBuffers.size(); i++) {
		mDebug("using input buffer %p with size %d", decoderBuffers[i].first, decoderBuffers[i].second);
		err = OMX_UseBuffer(handleScalar, &scInBufs[i], OMX_VFPC_INPUT_PORT_START_INDEX, this,
							decoderBuffers[i].second, decoderBuffers[i].first->pBuffer);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_UseBuffer()- Input Port State set : %s",
				   omxError(err));
			reterr = -ENOMEM;
			goto error_alloc_input_buffer;
		}
		bufMap.insert(decoderBuffers[i].first, scInBufs[i]);
		mDebug("scalar input buffer %p using %p", scInBufs[i], decoderBuffers[i].first);
	}
	/* allocate output buffers to pass to display */
	for (int i = 0; i < OUTPUT_BUFFER_COUNT; i++) {
		mDebug("allocating output buffer %d with size %d", i, outputBufferSize);
		err = OMX_AllocateBuffer(handleScalar, &scOutBufs[i], OMX_VFPC_OUTPUT_PORT_START_INDEX,
								 this, outputBufferSize);
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
		goto error_alloc_output_buffer;
	}
	mDebug("scalar buffers allocated");

	if (execute())
		goto error_scalar_cmd_exec;

	mDebug("omx scalar setted-up sucessfully");
	return 0;

error_scalar_cmd_exec:
	mDebug("freeing scalar output buffers");
	for (int i = 0; i < OUTPUT_BUFFER_COUNT; i++) {
		err = OMX_FreeBuffer(handleScalar, OMX_VFPC_OUTPUT_PORT_START_INDEX, scOutBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_FreeBuffer(): %s", omxError(err));
		}
	}
error_alloc_output_buffer:
	mDebug("freeing scalar input buffers");
	for (int i = 0; i < decoderBuffers.size(); i++) {
		err = OMX_FreeBuffer(handleScalar, OMX_VFPC_INPUT_PORT_START_INDEX, scInBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_FreeBuffer(): %s", omxError(err));
		}
	}
error_alloc_input_buffer:
error_scalar_cmd_idle:
error_enable_port:
error_alloc_sc_setparams:
	mDebug("freeing scalar handle");
	err = OMX_FreeHandle(handleScalar);
	if (err != OMX_ErrorNone)
		mDebug("error freeing scalar handle: %s", omxError(err));
error_alloc_schandle:
	mDebug("!!! error %d !!!", reterr);
	return reterr;
}

int OmxScaler::stopComponent()
{
	handleLock.lock();
	OMX_ERRORTYPE err = OMX_SendCommand(handleScalar, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting decoder state to idle: %s", omxError(err));
	} else
		waitOmxComp();

	err = OMX_SendCommand(handleScalar, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting decoder state to loaded: %s", omxError(err));
	}

	mDebug("freeing scalar input buffers");
	for (int i = 0; i < decoderBuffers.size(); i++) {
		err = OMX_FreeBuffer(handleScalar, OMX_VFPC_INPUT_PORT_START_INDEX, scInBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_FreeBuffer(): %s", omxError(err));
		}
	}
	mDebug("freeing decoder output buffers");
	for (int i = 0; i < OUTPUT_BUFFER_COUNT; i++) {
		err = OMX_FreeBuffer(handleScalar, OMX_VFPC_OUTPUT_PORT_START_INDEX, scOutBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_FreeBuffer(): %s", omxError(err));
		}
	}
	waitOmxComp();

	mDebug("freeing scalar handle");
	err = OMX_FreeHandle(handleScalar);
	if (err != OMX_ErrorNone)
		mDebug("error freeing decoder handle: %s", omxError(err));
	handleScalar = NULL;
	handleLock.unlock();
	return 0;
}

const QList<QPair<OMX_BUFFERHEADERTYPE *, int> > OmxScaler::getScalarBuffers()
{
	QList<QPair<OMX_BUFFERHEADERTYPE *, int> > list;
	for (int i = 0; i < OUTPUT_BUFFER_COUNT; i++) {
		list << QPair<OMX_BUFFERHEADERTYPE *, int>(scOutBufs[i], outputBufferSize);
	}
	return list;
}

void OmxScaler::handleInputBufferDone(OMX_BUFFERHEADERTYPE *omxBuf)
{
	bufLock.lock();
	if (!rawBufMap.contains(omxBuf)) {
		bufLock.unlock();
		return;
	}
	rawBufMap.remove(omxBuf);
	mInfo("received new input buffer done: count=%d input=%d", rawBufMap.size(), getInputBufferCount());
	bufLock.unlock();
}

void OmxScaler::handleDispBuffer(OMX_BUFFERHEADERTYPE *omxBuf)
{
	RawBuffer buf(this);
	buf.setRefData("video/x-raw-int", omxBuf->pBuffer, omxBuf->nFilledLen);
	buf.addBufferParameter("omxBuf", (int)omxBuf);
	bufLock.lock();
	if (busyOutBuffers.contains(omxBuf)) {
		availOutBuffers << omxBuf;
		busyOutBuffers.removeOne(omxBuf);
	}
	bufLock.unlock();

	newOutputBuffer(0, buf);
}


#define OMX_INIT_PARAM(param)                                                  \
		{                                                                      \
		  memset ((param), 0, sizeof (*(param)));                              \
		  (param)->nSize = sizeof (*(param));                                  \
		  (param)->nVersion.s.nVersionMajor = 1;                               \
		  (param)->nVersion.s.nVersionMinor = 1;                               \
		}

int OmxScaler::setOmxScalarParams()
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_PARAM_BUFFER_MEMORYTYPE memTypeCfg;
	OMX_PARAM_PORTDEFINITIONTYPE paramPort;
	OMX_PARAM_VFPC_NUMCHANNELPERHANDLE sNumChPerHandle;
	OMX_CONFIG_ALG_ENABLE algEnable;
	OMX_CONFIG_VIDCHANNEL_RESOLUTION chResolution;

	OMX_INIT_PARAM (&memTypeCfg);
	memTypeCfg.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
	memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
	eError =
			OMX_SetParameter (handleScalar, (OMX_INDEXTYPE)OMX_TI_IndexParamBuffMemType,
							  &memTypeCfg);

	if (eError != OMX_ErrorNone) {
		mDebug("failed to set memory Type at input port");
		return -EINVAL;
	}

	/* Setting Memory type at output port to Raw Memory */
	OMX_INIT_PARAM (&memTypeCfg);
	memTypeCfg.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
	memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
	eError =
			OMX_SetParameter (handleScalar, (OMX_INDEXTYPE)OMX_TI_IndexParamBuffMemType,
							  &memTypeCfg);

	if (eError != OMX_ErrorNone)
	{
		mDebug("failed to set memory Type at output port");
		return -EINVAL;
	}

	/* set input height/width and color format */
	OMX_INIT_PARAM (&paramPort);
	paramPort.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;

	OMX_GetParameter (handleScalar, OMX_IndexParamPortDefinition,
					  &paramPort);
	paramPort.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
	paramPort.format.video.nFrameWidth = width;
	paramPort.format.video.nFrameHeight = height;

	/* Scalar is connceted to H264 decoder, whose stride is different than width*/
	paramPort.format.video.nStride = videoStride;
	paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
	paramPort.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
	paramPort.nBufferSize = (paramPort.format.video.nStride * height * 3) / 2;

	paramPort.nBufferAlignment = 0;
	paramPort.bBuffersContiguous = (OMX_BOOL)0;
	paramPort.nBufferCountActual = decoderBuffers.size();
	mDebug("set input port params (width = %u, height = %u, stride = %u)", width, height, videoStride);
	eError = OMX_SetParameter (handleScalar, OMX_IndexParamPortDefinition,
							   &paramPort);
	if(eError != OMX_ErrorNone)
	{
		mDebug(" Invalid INPUT color formats for Scalar");
		//OMX_FreeHandle (pAppData->pDisHandle);
		return eError;
	}

	/* set output height/width and color format */
	OMX_INIT_PARAM (&paramPort);
	paramPort.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
	OMX_GetParameter (handleScalar, OMX_IndexParamPortDefinition,
					  &paramPort);

	paramPort.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
	paramPort.format.video.nFrameWidth = width;
	paramPort.format.video.nFrameHeight = height;
	paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
	paramPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
	paramPort.nBufferAlignment = 0;
	paramPort.bBuffersContiguous = (OMX_BOOL)0;
	paramPort.nBufferCountActual = OUTPUT_BUFFER_COUNT;
	/* scalar buffer pitch should be multiple of 16 */
	paramPort.format.video.nStride = ((width + 15) & 0xfffffff0) * 2;

	paramPort.nBufferSize =
			paramPort.format.video.nStride * paramPort.format.video.nFrameHeight;
	outputBufferSize = paramPort.nBufferSize;

	mDebug("set output port params (width = %u, height = %u)",
			(unsigned int)paramPort.format.video.nFrameWidth,
			(unsigned int)paramPort.format.video.nFrameHeight);

	eError = OMX_SetParameter (handleScalar, OMX_IndexParamPortDefinition,
							   &paramPort);
	if(eError != OMX_ErrorNone)
	{
		mDebug("Invalid OUTPUT color formats for Scalar");
		//OMX_FreeHandle (pAppData->pDisHandle);
		return eError;
	}

	/* set number of channles */
	mDebug("setting number of channels");

	OMX_INIT_PARAM (&sNumChPerHandle);
	sNumChPerHandle.nNumChannelsPerHandle = 1;
	eError =
			OMX_SetParameter (handleScalar,
							  (OMX_INDEXTYPE) OMX_TI_IndexParamVFPCNumChPerHandle,
							  &sNumChPerHandle);
	if (eError != OMX_ErrorNone)
	{
		mDebug("failed to set num of channels");
		return -EINVAL;
	}

	/* set VFPC input and output resolution information */
	mDebug("setting input resolution");

	OMX_INIT_PARAM (&chResolution);
	chResolution.Frm0Width = width;
	chResolution.Frm0Height = height;
	chResolution.Frm0Pitch = videoStride;
	chResolution.Frm1Width = 0;
	chResolution.Frm1Height = 0;
	chResolution.Frm1Pitch = 0;
	chResolution.FrmStartX = 0;
	chResolution.FrmStartY = 0;
	chResolution.FrmCropWidth = outputWidth;
	chResolution.FrmCropHeight = outputHeight;
	chResolution.eDir = OMX_DirInput;
	chResolution.nChId = 0;

	eError =
			OMX_SetConfig (handleScalar,
						   (OMX_INDEXTYPE) OMX_TI_IndexConfigVidChResolution,
						   &chResolution);
	if (eError != OMX_ErrorNone) {
		mDebug("failed to set input channel resolution");
		return -EINVAL;
	}

	mDebug ("setting output resolution");
	OMX_INIT_PARAM (&chResolution);
	chResolution.Frm0Width = outputWidth;
	chResolution.Frm0Height = outputHeight;
	chResolution.Frm0Pitch = ((outputWidth + 15) & 0xfffffff0) * 2;
	chResolution.Frm1Width = 0;
	chResolution.Frm1Height = 0;
	chResolution.Frm1Pitch = 0;
	chResolution.FrmStartX = 0;
	chResolution.FrmStartY = 0;
	chResolution.FrmCropWidth = 0;
	chResolution.FrmCropHeight = 0;
	chResolution.eDir = OMX_DirOutput;
	chResolution.nChId = 0;

	eError =
			OMX_SetConfig (handleScalar,
						   (OMX_INDEXTYPE) OMX_TI_IndexConfigVidChResolution,
						   &chResolution);
	if (eError != OMX_ErrorNone)
	{
		mDebug("failed to set output channel resolution");
		return -EINVAL;
	}

	/* disable algo bypass mode */
	OMX_INIT_PARAM (&algEnable);
	algEnable.nPortIndex = 0;
	algEnable.nChId = 0;
	algEnable.bAlgBypass = (OMX_BOOL)0;

	eError =
			OMX_SetConfig (handleScalar,
						   (OMX_INDEXTYPE) OMX_TI_IndexConfigAlgEnable, &algEnable);
	if (eError != OMX_ErrorNone) {
		mDebug("failed to disable algo by pass mode");
		return -EINVAL;
	}

	return (eError);
}
