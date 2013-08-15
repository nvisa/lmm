#ifndef BASEOMXELEMENT_H
#define BASEOMXELEMENT_H

#include <lmm/baselmmelement.h>

#include <ti/omx/interfaces/openMaxv11/OMX_Core.h>

#include <QMutex>
#include <QSemaphore>

typedef void* OMX_HANDLETYPE;
struct OMX_CALLBACKTYPE;
struct OMX_BUFFERHEADERTYPE;

class BaseOmxElement : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseOmxElement(QObject *parent = 0);
	int start();
	int stop();
	virtual int execute();
	/* TODO make private and use friends */
	int waitOmxComp();
	int omxCompDone();
	virtual int inputBufferDone(OMX_BUFFERHEADERTYPE *omxBuf);
	virtual int outputBufferDone(OMX_BUFFERHEADERTYPE *omxBuf);

	virtual void aboutDeleteBuffer(const QMap<QString, QVariant> &);
signals:
	void newOutputBufferDone();
	void newInputBufferDone();
protected slots:
	void handleDispBuffer();
	virtual void handleDispBuffer(OMX_BUFFERHEADERTYPE *omxBuf) = 0;
	void handleInputBufferDone();
	virtual void handleInputBufferDone(OMX_BUFFERHEADERTYPE *omxBuf) = 0;
protected:
	static QString getStateName(int nData2);
	static const char * omxError(OMX_ERRORTYPE error);

	virtual int startComponent() = 0;
	virtual int stopComponent() = 0;

	QSemaphore sem;
	OMX_CALLBACKTYPE *omxCallbacks;
	virtual OMX_HANDLETYPE getCompHandle() = 0;

	QMutex bufLock;
	QMutex bufLockEl;
	QMutex handleLock;
	QList<OMX_BUFFERHEADERTYPE *> elBuffers;
	QList<OMX_BUFFERHEADERTYPE *> elBuffersIn;
	QList<OMX_BUFFERHEADERTYPE *> availBuffers;
	QList<OMX_BUFFERHEADERTYPE *> availOutBuffers;
	QList<OMX_BUFFERHEADERTYPE *> busyBuffers;
	QList<OMX_BUFFERHEADERTYPE *> busyOutBuffers;
};

#endif // BASEOMXELEMENT_H
