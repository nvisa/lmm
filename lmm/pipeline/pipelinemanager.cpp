#include "pipelinemanager.h"

#include "debug.h"
#include "lmmcommon.h"
#include "baseplayer.h"
#include "baselmmpipeline.h"
#include "pipelinedebugger.h"

#include <errno.h>
#include <signal.h>

#include <QCoreApplication>

extern __thread LmmThread *currentLmmThread;
static __thread BaseLmmPipeline *currentPipeline = NULL;

PipelineManager::PipelineManager(QObject *parent) :
	BaseLmmElement(parent)
{
	quitting = false;
	dbg = new PipelineDebugger;

	LmmCommon::registerForSignal(SIGINT, this);
}

int PipelineManager::start()
{
	quitting = false;
	for (int i = 0; i < pipelines.size(); i++) {
		int err = pipelines[i]->start();
		if (err)
			return err;

		QString name = QString("MainPipelineThread%1").arg(i + 1);
		pipelineLock.lock();
		pipelinesByThread.insert(name, pipelines[i]);
		pipelineLock.unlock();
		createOpThread(&PipelineManager::pipelineThread, name, PipelineManager);
	}

	return BaseLmmElement::start();
}

int PipelineManager::stop()
{
	quitting = true;
	for (int i = 0; i < pipelines.size(); i++)
		pipelines[i]->stop();
	for (int i = 0; i < pipelines.size(); i++)
		pipelines[i]->waitForFinished(100);
	return BaseLmmElement::stop();
}

void PipelineManager::signalReceived(int sig)
{
	Q_UNUSED(sig);
}

int PipelineManager::processBuffer(const RawBuffer &buf)
{
	Q_UNUSED(buf);
	return 0;
}

BaseLmmPipeline *PipelineManager::addPipeline()
{
	BaseLmmPipeline *pipeline = new BaseLmmPipeline(this);
	pipeline->setObjectName(QString("Pipeline%1").arg(pipelines.size()));
	pipelines << pipeline;
	dbg->addPipeline(pipeline);
	return pipeline;
}

QList<BaseLmmElement *> PipelineManager::getElements()
{
	QList<BaseLmmElement *> list;
	for (int i = 0; i < pipelines.size(); i++) {
		int cnt = pipelines[i]->getPipeCount();
		for (int j = 0; j < cnt; j++)
			if (!list.contains(pipelines[i]->getPipe(j)))
				list << pipelines[i]->getPipe(j);
	}
	return list;
}

int PipelineManager::pipelineThread()
{
	if (quitting)
		return -EINVAL;
	if (!currentPipeline) {
		pipelineLock.lock();
		currentPipeline = pipelinesByThread[currentLmmThread->threadName()];
		pipelineLock.unlock();
	}
	RawBuffer buf = currentPipeline->nextBufferBlocking(0);
	return pipelineOutput(currentPipeline, buf);
}
