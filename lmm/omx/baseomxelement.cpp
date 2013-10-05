#include "baseomxelement.h"

#include <lmm/debug.h>

#include <xdc/std.h>

#include <ti/omx/interfaces/openMaxv11/OMX_Types.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Core.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Video.h>
#include <ti/omx/interfaces/openMaxv11/OMX_Component.h>
#include <ti/omx/interfaces/openMaxv11/OMX_TI_Common.h>
#include <ti/omx/interfaces/openMaxv11/OMX_TI_Index.h>
#include <ti/omx/interfaces/openMaxv11/OMX_TI_Video.h>

#include <errno.h>

static OMX_ERRORTYPE cb_EventHandler( OMX_HANDLETYPE hComponent,
									  OMX_PTR pAppData,
									  OMX_EVENTTYPE eEvent,
									  OMX_U32 nData1,
									  OMX_U32 nData2,
									  OMX_PTR pEventData)
{
	BaseOmxElement *el = (BaseOmxElement *)pAppData;
	if (eEvent == OMX_EventCmdComplete) {
		if (nData1 == OMX_CommandStateSet) {
			el->omxCompDone();
		} else if (OMX_CommandFlush == nData1) {
			el->omxCompDone();
		} else if (OMX_CommandPortEnable || OMX_CommandPortDisable) {
			//semp_post (comp->port_sem);
			el->omxCompDone();
		}
	} else if (eEvent == OMX_EventBufferFlag) {
		if ((int) nData2 == OMX_BUFFERFLAG_EOS) {
			//semp_post(comp->eos);
		}
	} else if (eEvent == OMX_EventError) {
	} else {
	}
	return OMX_ErrorNone;
}

static OMX_ERRORTYPE cb_EmptyBufferDone(
		OMX_IN OMX_HANDLETYPE hComponent,
		OMX_IN OMX_PTR pAppData,
		OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
	BaseOmxElement *el = (BaseOmxElement *)pAppData;
	el->inputBufferDone(pBuffer);
	return OMX_ErrorNone;
}

static OMX_ERRORTYPE cb_FillBufferDone(
		OMX_OUT OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_PTR pAppData,
		OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
	BaseOmxElement *el = (BaseOmxElement *)pAppData;
	el->outputBufferDone(pBuffer);
	return OMX_ErrorNone;
}

BaseOmxElement::BaseOmxElement(QObject *parent) :
	BaseLmmElement(parent)
{
	omxCallbacks = new OMX_CALLBACKTYPE;
	omxCallbacks->EmptyBufferDone = cb_EmptyBufferDone;
	omxCallbacks->EventHandler = cb_EventHandler;
	omxCallbacks->FillBufferDone = cb_FillBufferDone;
}

int BaseOmxElement::start()
{
	connect(this, SIGNAL(newInputBufferDone()), this, SLOT(handleInputBufferDone()), Qt::QueuedConnection);
	connect(this, SIGNAL(newOutputBufferDone()), this, SLOT(handleDispBuffer()), Qt::QueuedConnection);
	int err = startComponent();
	if (err)
		return err;
	return BaseLmmElement::start();
}

int BaseOmxElement::stop()
{
	disconnect(this, SIGNAL(newInputBufferDone()), this, SLOT(handleInputBufferDone()));
	disconnect(this, SIGNAL(newOutputBufferDone()), this, SLOT(handleDispBuffer()));
	mDebug("stopping component");
	int err = stopComponent();
	if (err)
		return err;
	mDebug("component stopped");
	return BaseLmmElement::stop();
}

int BaseOmxElement::execute()
{
	return 0;
}

int BaseOmxElement::waitOmxComp()
{
	if (!sem.tryAcquire(1, 1000))
		return -ENOENT;
	return 0;
}

int BaseOmxElement::omxCompDone()
{
	sem.release();
	return 0;
}

int BaseOmxElement::inputBufferDone(OMX_BUFFERHEADERTYPE *omxBuf)
{
	bufLockEl.lock();
	elBuffersIn << omxBuf;
	bufLockEl.unlock();
	emit newInputBufferDone();

	return 0;
}

int BaseOmxElement::outputBufferDone(OMX_BUFFERHEADERTYPE *omxBuf)
{
	bufLockEl.lock();
	elBuffers << omxBuf;
	bufLockEl.unlock();
	emit newOutputBufferDone();
	return 0;
}

void BaseOmxElement::aboutDeleteBuffer(const QHash<QString, QVariant> &pars)
{
	OMX_BUFFERHEADERTYPE *omxBuf = (OMX_BUFFERHEADERTYPE *)pars["omxBuf"].toInt();
	mInfo("giving buffer %p back to omx component", omxBuf);
	bufLock.lock();
	availOutBuffers.removeOne(omxBuf);
	busyOutBuffers << omxBuf;
	bufLock.unlock();
	handleLock.lock();
	if (getCompHandle())
		OMX_FillThisBuffer(getCompHandle(), omxBuf);
	handleLock.unlock();
}

QList<QVariant> BaseOmxElement::extraDebugInfo()
{
	QList<QVariant> list;
	bufLock.lock();
	list << availBuffers.size();
	list << availOutBuffers.size();
	list << busyBuffers.size();
	list << busyOutBuffers.size();
	bufLock.unlock();
	return list;
}

const char * BaseOmxElement::omxError(OMX_ERRORTYPE error)
{
	/* used for printing purpose */
	switch (error)
	{
	case OMX_ErrorNone:
		return "OMX_ErrorNone";
		break;
	case OMX_ErrorInsufficientResources:
		return "OMX_ErrorInsufficientResources";
		break;
	case OMX_ErrorUndefined:
		return "OMX_ErrorUndefined";
		break;
	case OMX_ErrorInvalidComponentName:
		return "OMX_ErrorInvalidComponentName";
		break;
	case OMX_ErrorComponentNotFound:
		return "OMX_ErrorComponentNotFound";
		break;
	case OMX_ErrorInvalidComponent:
		return "OMX_ErrorInvalidComponent";
		break;
	case OMX_ErrorBadParameter:
		return "OMX_ErrorBadParameter";
		break;
	case OMX_ErrorNotImplemented:
		return "OMX_ErrorNotImplemented";
		break;
	case OMX_ErrorUnderflow:
		return "OMX_ErrorUnderflow";
		break;
	case OMX_ErrorOverflow:
		return "OMX_ErrorOverflow";
		break;
	case OMX_ErrorHardware:
		return "OMX_ErrorHardware";
		break;
	case OMX_ErrorInvalidState:
		return "OMX_ErrorInvalidState";
		break;
	case OMX_ErrorStreamCorrupt:
		return "OMX_ErrorStreamCorrupt";
		break;
	case OMX_ErrorPortsNotCompatible:
		return "OMX_ErrorPortsNotCompatible";
		break;
	case OMX_ErrorResourcesLost:
		return "OMX_ErrorResourcesLost";
		break;
	case OMX_ErrorNoMore:
		return "OMX_ErrorNoMore";
		break;
	case OMX_ErrorVersionMismatch:
		return "OMX_ErrorVersionMismatch";
		break;
	case OMX_ErrorNotReady:
		return "OMX_ErrorNotReady";
		break;
	case OMX_ErrorTimeout:
		return "OMX_ErrorTimeout";
		break;
	default:
		return "<unknown>";
	}

	return "";
}

QString BaseOmxElement::getStateName(int nData2)
{
	switch (nData2)
	{
	case OMX_StateInvalid:
		return QString("OMX_StateInvalid");
		break;
	case OMX_StateLoaded:
		return QString ("OMX_StateLoaded");
		break;
	case OMX_StateIdle:
		return QString ("OMX_StateIdle");
		break;
	case OMX_StateExecuting:
		return QString ("OMX_StateExecuting \n");
		break;
	case OMX_StatePause:
		return QString ("OMX_StatePause\n");
		break;
	case OMX_StateWaitForResources:
		return QString ("OMX_StateWaitForResources\n");
		break;
	}
	return QString("");
}


void BaseOmxElement::handleDispBuffer()
{
	mInfo("received new output buffer from component");
	bufLockEl.lock();
	OMX_BUFFERHEADERTYPE *omxBuf = NULL;
	if (elBuffers.size())
		omxBuf = elBuffers.takeFirst();
	bufLockEl.unlock();

	if (!omxBuf)
		return;

	handleDispBuffer(omxBuf);
}

void BaseOmxElement::handleInputBufferDone()
{
	mLog("received new input buffer done");
	bufLockEl.lock();
	OMX_BUFFERHEADERTYPE *omxBuf = NULL;
	if (elBuffersIn.size())
		omxBuf = elBuffersIn.takeFirst();
	bufLockEl.unlock();

	if (!omxBuf)
		return;

	handleInputBufferDone(omxBuf);
}
