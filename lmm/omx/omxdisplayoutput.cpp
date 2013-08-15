#include "omxdisplayoutput.h"

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
#include <ti/omx/comp/vfdc/omx_vfdc.h>
#include <ti/omx/comp/vfpc/omx_vfpc.h>

OmxDisplayOutput::OmxDisplayOutput(QObject *parent) :
	BaseOmxElement(parent)
{
	width = 1920;
	height = 1080;
	dispWidth = 1920;
	dispHeight = 1080;
}

int OmxDisplayOutput::setFrameSize(QSize sz)
{
	width = sz.width();
	height = sz.height();
	return 0;
}

int OmxDisplayOutput::displayBlocking()
{
	if (!acquireInputSem(0))
		return -EINVAL;
	inputLock.lock();
	RawBuffer buf = inputBuffers.takeFirst();
	inputLock.unlock();
	return display(buf);
}

int OmxDisplayOutput::display(RawBuffer buf)
{
	OMX_BUFFERHEADERTYPE *omxBuf = (OMX_BUFFERHEADERTYPE *)buf.getBufferParameter("omxBuf").toInt();
	if (!omxBuf) {
		mDebug("no omx buffers available");
		return -EINVAL;
	}
	OMX_BUFFERHEADERTYPE *mybuf = bufMap[omxBuf];
	handleLock.lock();
	if (!handleDisp) {
		handleLock.unlock();
		return -EINVAL;
	}
	mybuf->nFilledLen = omxBuf->nFilledLen;
	mybuf->nOffset = omxBuf->nOffset;
	mybuf->nTimeStamp = omxBuf->nTimeStamp;
	mybuf->nFlags = omxBuf->nFlags;
	mybuf->hMarkTargetComponent = omxBuf->hMarkTargetComponent;
	mybuf->pMarkData = omxBuf->pMarkData;
	mybuf->nTickCount = omxBuf->nTickCount;
	OMX_ERRORTYPE err = OMX_EmptyThisBuffer(handleDisp, mybuf);
	handleLock.unlock();
	if (err != OMX_ErrorNone) {
		mDebug("error passing buffer %p(%p) to omx component: %s", mybuf, omxBuf, omxError(err));
		return -EINVAL;
	}
	bufLock.lock();
	rawBufMap.insert(mybuf, buf);
	bufLock.unlock();

	if (1) {
		sentBufferCount++;
		calculateFps();
		updateOutputTimeStats();
	}
	return 0;
}

int OmxDisplayOutput::startComponent()
{
	int reterr = 0;
	handleDisp = NULL;
	OMX_ERRORTYPE err = OMX_GetHandle(&handleDisp, (OMX_STRING) "OMX.TI.VPSSM3.VFDC", this, omxCallbacks);
	mInfo("display component is created");
	if ((err != OMX_ErrorNone) || (handleDisp == NULL)) {
		mDebug("Error in Get Handle function : %s", omxError(err));
		reterr = -ENOENT;
		goto error_alloc_disphandle;
	}

	err = OMX_GetHandle(&handleCtrl, (OMX_STRING) "OMX.TI.VPSSM3.CTRL.DC", this, omxCallbacks);
	mInfo("display controller component is created");
	if ((err != OMX_ErrorNone) || (handleCtrl == NULL)) {
		mDebug("Error in Get Handle function : %s", omxError(err));
		reterr = -ENOENT;
		goto error_alloc_disphandle;
	}

	err = (OMX_ERRORTYPE)setOmxDisplayParams();
	if (err != OMX_ErrorNone) {
		mDebug("Error in setOmxDecoderParams() function : %s", omxError(err));
		reterr = -EINVAL;
		goto error_alloc_disp_setparams;
	}
	mDebug("display params set successfully");

	OMX_SendCommand(handleDisp, OMX_CommandPortEnable, OMX_VFDC_INPUT_PORT_START_INDEX, NULL);
	if (waitOmxComp()) {
		mDebug("error waiting for component to enable input port");
		reterr = -EBUSY;
		goto error_enable_port;
	}

	err = OMX_SendCommand(handleCtrl, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting display controller state to idle: %s", omxError(err));
		reterr = -EINVAL;
		goto error_display_cmd_idle;
	}
	mDebug("display controller state set to idle");
	if (waitOmxComp()) {
		mDebug("error waiting for component to go idle state");
		reterr = -EBUSY;
		goto error_alloc_input_buffer;
	}

	err = OMX_SendCommand(handleDisp, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting display state to idle: %s", omxError(err));
		reterr = -EINVAL;
		goto error_display_cmd_idle;
	}
	mDebug("display state set to idle");

	for (int i = 0; i < scalarBuffers.size(); i++) {
		mDebug("using input buffer %p with size %d", scalarBuffers[i].first, scalarBuffers[i].second);
		err = OMX_UseBuffer(handleDisp, &dispInBufs[i], OMX_VFDC_INPUT_PORT_START_INDEX, this,
							scalarBuffers[i].second, scalarBuffers[i].first->pBuffer);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_UseBuffer()- Input Port State set : %s",
				   omxError(err));
			reterr = -ENOMEM;
			goto error_alloc_input_buffer;
		}
		bufMap.insert(scalarBuffers[i].first, dispInBufs[i]);
		mDebug("display input buffer %p using %p", dispInBufs[i], scalarBuffers[i].first);
	}
	if (waitOmxComp()) {
		mDebug("error waiting for component to go idle state");
		reterr = -EBUSY;
		goto error_alloc_input_buffer;
	}
	mDebug("display buffers allocated");

	if (execute())
		goto error_display_cmd_exec;

	mDebug("display component setted-up successfully");
	return 0;

error_display_cmd_exec:
	mDebug("freeing display input buffers");
	for (int i = 0; i < scalarBuffers.size(); i++) {
		err = OMX_FreeBuffer(handleDisp, OMX_VFDC_INPUT_PORT_START_INDEX, dispInBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_FreeBuffer(): %s", omxError(err));
		}
	}
error_alloc_input_buffer:
error_display_cmd_idle:
error_enable_port:
error_alloc_disp_setparams:
	mDebug("freeing display handle");
	err = OMX_FreeHandle(handleDisp);
	if (err != OMX_ErrorNone)
		mDebug("error freeing display handle: %s", omxError(err));
error_alloc_disphandle:
	mDebug("!!! error %d !!!", reterr);
	return reterr;
}

int OmxDisplayOutput::stopComponent()
{
	handleLock.lock();
	if (!handleDisp) {
		handleLock.unlock();
		return 0;
	}
	OMX_ERRORTYPE err = OMX_SendCommand(handleDisp, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting display state to idle: %s", omxError(err));
	} else
		waitOmxComp();

	err = OMX_SendCommand(handleCtrl, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting display controller state to idle: %s", omxError(err));
	} else
		waitOmxComp();

	err = OMX_SendCommand(handleDisp, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting component state to loaded: %s", omxError(err));
	}

	mDebug("freeing display input buffers");
	for (int i = 0; i < scalarBuffers.size(); i++) {
		err = OMX_FreeBuffer(handleDisp, OMX_VFDC_INPUT_PORT_START_INDEX, dispInBufs[i]);
		if (err != OMX_ErrorNone) {
			mDebug("Error in OMX_FreeBuffer(): %s", omxError(err));
		}
	}
	waitOmxComp();

	err = OMX_SendCommand(handleCtrl, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting component state to loaded: %s", omxError(err));
	}
	waitOmxComp();

	mDebug("freeing display handle");
	err = OMX_FreeHandle(handleDisp);
	if (err != OMX_ErrorNone)
		mDebug("error freeing disp handle: %s", omxError(err));
	err = OMX_FreeHandle(handleCtrl);
	if (err != OMX_ErrorNone)
		mDebug("error freeing ctrl handle: %s", omxError(err));
	handleDisp = NULL;
	handleCtrl = NULL;
	handleLock.unlock();
	return 0;
}

void OmxDisplayOutput::handleDispBuffer(OMX_BUFFERHEADERTYPE *omxBuf)
{
}

void OmxDisplayOutput::handleInputBufferDone(OMX_BUFFERHEADERTYPE *omxBuf)
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

#define OMX_INIT_PARAM(param)                                                  \
		{                                                                      \
		  memset ((param), 0, sizeof (*(param)));                              \
		  (param)->nSize = sizeof (*(param));                                  \
		  (param)->nVersion.s.nVersionMajor = 1;                               \
		  (param)->nVersion.s.nVersionMinor = 1;                               \
		}

int OmxDisplayOutput::setOmxDisplayParams()
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_PARAM_BUFFER_MEMORYTYPE memTypeCfg;
	OMX_PARAM_PORTDEFINITIONTYPE paramPort;
	OMX_PARAM_VFDC_DRIVERINSTID driverId;
	OMX_PARAM_VFDC_CREATEMOSAICLAYOUT mosaicLayout;
	OMX_CONFIG_VFDC_MOSAICLAYOUT_PORT2WINMAP port2Winmap;

	OMX_INIT_PARAM (&paramPort);

	/* set input height/width and color format */
	paramPort.nPortIndex = OMX_VFDC_INPUT_PORT_START_INDEX;
	OMX_GetParameter (handleDisp, OMX_IndexParamPortDefinition,
					  &paramPort);
	paramPort.nPortIndex = OMX_VFDC_INPUT_PORT_START_INDEX;
	paramPort.format.video.nFrameWidth = width;
	paramPort.format.video.nFrameHeight = height;
	paramPort.format.video.nStride = ((width + 15) & 0xfffffff0) * 2;
	paramPort.nBufferCountActual = scalarBuffers.size();
	paramPort.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
	paramPort.format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;

	paramPort.nBufferSize = paramPort.format.video.nStride * paramPort.format.video.nFrameHeight;

	mDebug("Buffer Size computed: %d", (int) paramPort.nBufferSize);
	mDebug("set input port params (width = %d, height = %d)",width, height);

	eError =
			OMX_SetParameter (handleDisp, OMX_IndexParamPortDefinition,
							  &paramPort);
	if(eError != OMX_ErrorNone)
	{
		mDebug("failed to set display params");
		return -EINVAL;
	}
	/* --------------------------------------------------------------------------*
		 Supported display IDs by VFDC and DC are below The names will be renamed in
		 future releases as some of the driver names & interfaces will be changed in
		 future @ param OMX_VIDEO_DISPLAY_ID_HD0: 422P On-chip HDMI @ param
		 OMX_VIDEO_DISPLAY_ID_HD1: 422P HDDAC component output @ param
		 OMX_VIDEO_DISPLAY_ID_SD0: 420T/422T SD display (NTSC): Not supported yet.
		 ------------------------------------------------------------------------ */

	/* set the parameter to the disaply component to 1080P @60 mode */
	OMX_INIT_PARAM (&driverId);
	/* Configured to use on-chip HDMI */
	driverId.nDrvInstID = OMX_VIDEO_DISPLAY_ID_HD0;
	driverId.eDispVencMode = OMX_DC_MODE_1080P_60;

	eError =
			OMX_SetParameter (handleDisp,
							  (OMX_INDEXTYPE) OMX_TI_IndexParamVFDCDriverInstId,
							  &driverId);
	if (eError != OMX_ErrorNone)
	{
		mDebug("failed to set driver mode to 1080P@60\n");
		return -EINVAL;
	}

	/* set the parameter to the disaply controller component to 1080P @60 mode */
	OMX_INIT_PARAM (&driverId);
	/* Configured to use on-chip HDMI */
	driverId.nDrvInstID = OMX_VIDEO_DISPLAY_ID_HD0;
	driverId.eDispVencMode = OMX_DC_MODE_1080P_60;

	eError =
			OMX_SetParameter (handleCtrl,
							  (OMX_INDEXTYPE) OMX_TI_IndexParamVFDCDriverInstId,
							  &driverId);
	if (eError != OMX_ErrorNone)
	{
		mDebug("failed to set driver mode to 1080P@60\n");
		return -EINVAL;
	}
	/* set mosaic layout info */
	if (1) {

		OMX_INIT_PARAM (&mosaicLayout);
		/* Configuring the first (and only) window */
		/* position of window can be changed by following cordinates, keeping in
		 center by default */

		mosaicLayout.sMosaicWinFmt[0].winStartX = (dispWidth - width) / 2;
		mosaicLayout.sMosaicWinFmt[0].winStartY = (dispHeight - height) / 2;
		mosaicLayout.sMosaicWinFmt[0].winWidth = width;
		mosaicLayout.sMosaicWinFmt[0].winHeight = height;
		mosaicLayout.sMosaicWinFmt[0].pitch[VFDC_YUV_INT_ADDR_IDX] =
				((width + 15) & 0xfffffff0) * 2;
		mosaicLayout.sMosaicWinFmt[0].dataFormat = VFDC_DF_YUV422I_YVYU;
		mosaicLayout.sMosaicWinFmt[0].bpp = VFDC_BPP_BITS16;
		mosaicLayout.sMosaicWinFmt[0].priority = 0;
		mosaicLayout.nDisChannelNum = 0;
		/* Only one window in this layout, hence setting it to 1 */
		mosaicLayout.nNumWindows = 1;

		eError = OMX_SetParameter (handleDisp, (OMX_INDEXTYPE)
								   OMX_TI_IndexParamVFDCCreateMosaicLayout,
								   &mosaicLayout);
		if (eError != OMX_ErrorNone)
		{
			mDebug("failed to set mosaic window parameter\n");
			return -EINVAL;
		}

		/* map OMX port to window */
		OMX_INIT_PARAM (&port2Winmap);
		/* signifies the layout id this port2win mapping refers to */
		port2Winmap.nLayoutId = 0;
		/* Just one window in this layout, hence setting the value to 1 */
		port2Winmap.numWindows = 1;
		/* Only 1st input port used here */
		port2Winmap.omxPortList[0] = OMX_VFDC_INPUT_PORT_START_INDEX + 0;
		eError =
				OMX_SetConfig (handleDisp,
							   (OMX_INDEXTYPE) OMX_TI_IndexConfigVFDCMosaicPort2WinMap,
							   &port2Winmap);
		if (eError != OMX_ErrorNone)
		{
			mDebug("failed to map port to windows\n");
			return -EINVAL;
		}
	}
	/* Setting Memory type at input port to Raw Memory */
	mDebug("setting input and output memory type to default\n");
	OMX_INIT_PARAM (&memTypeCfg);
	memTypeCfg.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
	memTypeCfg.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
	eError =
			OMX_SetParameter (handleDisp, (OMX_INDEXTYPE)OMX_TI_IndexParamBuffMemType,
							  &memTypeCfg);

	if (eError != OMX_ErrorNone)
	{
		mDebug("failed to set memory Type at input port\n");
		return -EINVAL;
	}

	return (eError);
}

int OmxDisplayOutput::execute()
{
	OMX_ERRORTYPE err = OMX_SendCommand(handleCtrl, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting display controller state to executing: %s", omxError(err));
		return -EINVAL;
	}
	if (waitOmxComp()) {
		mDebug("error waiting for component to go executing state");
		return -EBUSY;
	}

	err = OMX_SendCommand(handleDisp, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	if (err != OMX_ErrorNone) {
		mDebug("error setting display state to executing: %s", omxError(err));
		return -EINVAL;
	}
	if (waitOmxComp()) {
		mDebug("error waiting for component to go executing state");
		return -EBUSY;
	}
	return 0;
}


int OmxDisplayOutput::setDisplaySize(QSize sz)
{
	dispWidth = sz.width();
	dispHeight = sz.height();
	return 0;
}
