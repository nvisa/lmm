#ifndef BASELMMPIPELINE_H
#define BASELMMPIPELINE_H

#include <lmm/baselmmelement.h>

class BasePipe;
class QEventLoop;
class BasePipeElement;

class BaseLmmPipeline : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmPipeline(QObject *parent = 0);
	~BaseLmmPipeline();
	virtual BasePipeElement * appendPipe(BaseLmmElement *) { return NULL; }
	virtual BasePipeElement * addPipe(BaseLmmElement *, BaseLmmElement * = NULL) { return NULL; }
	virtual int start();
	virtual int stop();
	int getPipeCount();
	BaseLmmElement *getPipe(int off);
	void setPipelineReady(bool v);
	bool isPipelineReady();
	const QList<LmmThread *> getThreads();

	/* new API */
	int append(BaseLmmElement *el, int inputCh = 0);
	int appendFinal(BaseLmmElement *el, int inputCh = 0);
	int end();

	void waitForFinished(int timeout);

	virtual void threadFinished(LmmThread *);
protected:
	virtual int processBuffer(const RawBuffer &buf);
	virtual int processBuffer(int ch, const RawBuffer &buf);

	QMap<QString, LmmThread *> threads;
	QList<BaseLmmElement *> pipesNew;
	QMutex thLock;
	int finishedThreadCount;
	bool pipelineReady;
};

#endif // BASELMMPIPELINE_H
