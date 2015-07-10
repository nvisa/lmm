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
	virtual BasePipeElement * appendPipe(BaseLmmElement *el);
	virtual BasePipeElement * addPipe(BaseLmmElement *, BaseLmmElement *next = NULL);
	virtual int start();
	virtual int stop();

	/* */
	int processPipeline();
	int checkPipelineOutput();

	void waitToFinish();

	virtual void threadFinished(LmmThread *);
protected:
	virtual int processBuffer(const RawBuffer &buf);
	virtual int processBuffer(int ch, const RawBuffer &buf);

	QMap<QString, LmmThread *> threads;
	//QList<BaseLmmElement *> pipelineElements;
	QList<BasePipeElement *> pipes;
	QMutex thLock;
	QEventLoop *el;
	int finishedThreadCount;
};

#endif // BASELMMPIPELINE_H
