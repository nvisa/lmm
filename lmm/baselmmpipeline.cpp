#include "baselmmpipeline.h"
#include "baseplayer.h"
#include "debug.h"
#include "pipeline/basepipeelement.h"

#include <QEventLoop>

#include <errno.h>

BaseLmmPipeline::BaseLmmPipeline(QObject *parent) :
	BaseLmmElement(parent),
	pipelineReady(false)
{
	addNewInputChannel();
	el = new QEventLoop(this);
	pipelineReady = false;
}

BaseLmmPipeline::~BaseLmmPipeline()
{
	if (threads.size()) {
		mDebug("destroying threads");
		qDeleteAll(threads);
		threads.clear();
	}
}

BasePipeElement *BaseLmmPipeline::appendPipe(BaseLmmElement *el)
{
	BasePipeElement *pipeEl = new BasePipeElement(this);
	pipeEl->setPipe(el, this, 0, 0, 1);
	if (pipes.size()) {
		pipes.last()->setNext(pipeEl);
		pipes.last()->setNextChannel(0);
	}
	pipes << pipeEl;
	return pipeEl;
}

BasePipeElement * BaseLmmPipeline::addPipe(BaseLmmElement *el, BaseLmmElement *next)
{
	BasePipeElement *pipeEl = new BasePipeElement(this);
	pipeEl->setPipe(el, next);
	pipes << pipeEl;
	return pipeEl;
}

int BaseLmmPipeline::start()
{
	if (threads.size()) {
		mDebug("destroying threads");
		qDeleteAll(threads);
		threads.clear();
	}

	finishedThreadCount = 0;

	pipes.last()->setNext(this);
	pipes.last()->setNextChannel(1);

	for (int i = 0; i < pipes.size(); i++) {
		BasePipeElement *pipe = pipes[i];
		const struct BasePipeElement::Link link = pipe->getLink();
		/* create process thread */
		QString desc = link.source->objectName();
		if (desc.isEmpty())
			desc = link.source->metaObject()->className();
		if (link.sourceProcessChannel >= 0) {
			LmmThread *th = new OpThread<BasePipeElement>(pipe, &BasePipeElement::operationProcess,
														  objectName().append("P%1%2").arg(i).arg(desc));
			threads.insert(th->threadName(), th);
			th->start(QThread::LowestPriority);
		}

		/* create buffer thread */
		LmmThread *th = new OpThread<BasePipeElement>(pipe, &BasePipeElement::operationBuffer,
										   objectName().append("B%1%2").arg(i).arg(desc));
		threads.insert(th->threadName(), th);
		th->start(QThread::LowestPriority);
	}
	/* pipeline process thread */
	QString name = objectName().append("PCheck");
	LmmThread *th = new OpThread<BaseLmmPipeline>(this, &BaseLmmPipeline::processPipeline, name);
	threads.insert(name, th);
	th->start(QThread::LowestPriority);
	/* pipeline output check thread */
	name = objectName().append("OCheck");
	th = new OpThread<BaseLmmPipeline>(this, &BaseLmmPipeline::checkPipelineOutput, name);
	threads.insert(name, th);
	th->start(QThread::LowestPriority);

	return BaseLmmElement::start();
}

int BaseLmmPipeline::stop()
{
	return BaseLmmElement::stop();
}

BasePipeElement *BaseLmmPipeline::getPipe(int off)
{
	return pipes[off];
}

void BaseLmmPipeline::setPipelineReady(bool v)
{
	pipelineReady = v;
}

bool BaseLmmPipeline::isPipelineReady()
{
	return pipelineReady;
}

const QList<LmmThread *> BaseLmmPipeline::getThreads()
{
	return threads.values();
}

int BaseLmmPipeline::processPipeline()
{
	return processBlocking();
	/*int err = processBlocking();
	if (err == -ENODATA)
		processBuffer(RawBuffer::eof());
	return err;*/
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

int BaseLmmPipeline::processBuffer(const RawBuffer &buf)
{
	/* put buffer into the pipeline for processing */
	return pipes.first()->addBuffer(0, buf);
}

int BaseLmmPipeline::processBuffer(int ch, const RawBuffer &buf)
{
	Q_UNUSED(ch);
	/* we have new buffer output from pipeline */
	return newOutputBuffer(0, buf);
}
