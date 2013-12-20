#include "baselmmpipeline.h"
#include "baseplayer.h"
#include "debug.h"
#include "pipeline/basepipeelement.h"

#include <QEventLoop>

#include <errno.h>

/*class BasePipe : public BaseLmmElement
{
	Q_OBJECT
public:
	BasePipe(BaseLmmElement *el, BaseLmmElement *nextEl = NULL)
	{
		target = el;
		next = nextEl;
		targetCh = 0;
		nextCh = 0;
	}

	int operation()
	{
		if (next)
			return operationBuffer();
		return operationProcess();
	}

	int operationProcess()
	{
		return target->processBlocking(targetCh);
	}

	int operationBuffer()
	{
		RawBuffer buf = target->nextBufferBlocking(targetCh);
		if (!buf.size()) {
			return -ENOENT;
		}
		if (buf.isEOF()) {
			return -ENODATA;
		}
		return next->addBuffer(nextCh, buf);
	}
	BaseLmmElement *target;
	BaseLmmElement *next;
	int targetCh;
	int nextCh;
protected:
	int processBuffer(RawBuffer buf)
	{
		Q_UNUSED(buf);
		return 0;
	}
};*/

BaseLmmPipeline::BaseLmmPipeline(QObject *parent) :
	BaseLmmElement(parent)
{
	addNewInputChannel();
	el = new QEventLoop(this);
}

int BaseLmmPipeline::addPipe(BaseLmmElement *el)
{
	pipelineElements << el;
	return 0;
}

int BaseLmmPipeline::start()
{
	finishedThreadCount = 0;
	/*for (int i = 0; i < pipelineElements.size(); i++) {
		BaseLmmElement *next = this;
		if (i < pipelineElements.size() - 1)
			next = pipelineElements[i + 1];
		BasePipeElement *pipe = new BasePipeElement(this);
		pipe->setPipe(pipelineElements[i], next);
		if (next == this)
			pipe->setNextChannel(1);
		pipes << pipe;
	}*/

	pipes.last()->setNext(this);
	pipes.last()->setNextChannel(1);

	for (int i = 0; i < pipes.size(); i++) {
		BasePipeElement *pipe = pipes[i];
		/* create process thread */
		QString name = QString("BaseLmmPipelineProcessThread%1").arg(i);
		LmmThread *th = new OpThread<BasePipeElement>(pipe, &BasePipeElement::operationProcess, name);
		threads.insert(name, th);
		th->start(QThread::LowestPriority);

		/* create buffer thread */
		name = QString("BaseLmmPipelineBufferThread%1").arg(i + 1);
		th = new OpThread<BasePipeElement>(pipe, &BasePipeElement::operationBuffer, name);
		threads.insert(name, th);
		th->start(QThread::LowestPriority);
	}
	/* pipeline process thread */
	QString name = QString("PipelineProcessThread");
	LmmThread *th = new OpThread<BaseLmmPipeline>(this, &BaseLmmPipeline::processPipeline, name);
	threads.insert(name, th);
	th->start(QThread::LowestPriority);
	/* pipeline output check thread */
	name = QString("PipelineOutputCheckThread");
	th = new OpThread<BaseLmmPipeline>(this, &BaseLmmPipeline::checkPipelineOutput, name);
	threads.insert(name, th);
	th->start(QThread::LowestPriority);

	return BaseLmmElement::start();
}

int BaseLmmPipeline::stop()
{
	return BaseLmmElement::stop();
}

int BaseLmmPipeline::processPipeline()
{
	int err = processBlocking();
	if (err == -ENODATA)
		processBuffer(RawBuffer::eof());
	return err;
}

int BaseLmmPipeline::checkPipelineOutput()
{
	return processBlocking(1);
}

void BaseLmmPipeline::waitToFinish()
{
	thLock.lock();
	if (finishedThreadCount != threads.size()) {
		thLock.unlock();
		el->exec();
		return;
	}
	thLock.unlock();
}

void BaseLmmPipeline::threadFinished(LmmThread *)
{
	thLock.lock();
	mDebug("thread finished: %d %d", finishedThreadCount, threads.size());
	if (++finishedThreadCount == threads.size()) {
		el->quit();
	}
	thLock.unlock();
}

int BaseLmmPipeline::processBuffer(RawBuffer buf)
{
	/* put buffer into the pipeline for processing */
	return pipes.first()->addBuffer(0, buf);
}

int BaseLmmPipeline::processBuffer(int ch, RawBuffer buf)
{
	if (buf.isEOF())
		return -ENODATA;
	/* we have new buffer output from pipeline */
	return 0;
}

int BaseLmmPipeline::addPipe(BasePipe *pipe)
{
	BasePipeElement *pipeEl = new BasePipeElement(this);
	pipeEl->setPipe(pipe);
	if (pipes.size())
		pipes.last()->setNext(pipeEl);
	pipes << pipeEl;
	return 0;
}
