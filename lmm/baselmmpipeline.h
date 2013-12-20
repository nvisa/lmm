#ifndef BASELMMPIPELINE_H
#define BASELMMPIPELINE_H

#include <lmm/baselmmelement.h>

class BasePipe;
class QEventLoop;
class BasePipeElement;

/*template <class T>
class PipeOperation
{
public:
	typedef int (T::*pipeOp)();
	PipeOperation(T *parent, pipeOp op)
	{
		pclass = parent;
		mfunc = op;
	}
	T *pclass;
	pipeOp mfunc;
};*/

class BaseLmmPipeline : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmPipeline(QObject *parent = 0);
	virtual int addPipe(BaseLmmElement *el);
	virtual int addPipe(BasePipe *pipe);
	virtual int start();
	virtual int stop();

	/* */
	int processPipeline();
	int checkPipelineOutput();

	void waitToFinish();

	virtual void threadFinished(LmmThread *);
protected:
	virtual int processBuffer(RawBuffer buf);
	virtual int processBuffer(int ch, RawBuffer buf);

	QMap<QString, LmmThread *> threads;
	QList<BaseLmmElement *> pipelineElements;
	QList<BasePipeElement *> pipes;
	QMutex thLock;
	QEventLoop *el;
	int finishedThreadCount;
};

#endif // BASELMMPIPELINE_H
