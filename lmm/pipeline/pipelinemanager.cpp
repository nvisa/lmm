#include "pipelinemanager.h"

#include "debug.h"
#include "lmmcommon.h"
#include "baseplayer.h"
#include "baselmmpipeline.h"
#include "pipelinedebugger.h"

#include <errno.h>
#include <signal.h>

#include <QElapsedTimer>
#include <QCoreApplication>

extern __thread LmmThread *currentLmmThread;
static __thread BaseLmmPipeline *currentPipeline = NULL;

PipelineManager::PipelineManager(QObject *parent) :
	BaseLmmElement(parent)
{
	quitting = false;
	dbg = PipelineDebugger::GetInstance();
	QTimer::singleShot(1000, this, SLOT(timeout()));
	checkPipelineWdts = false;
}

int PipelineManager::start()
{
	quitting = false;
	for (int i = 0; i < pipelines.size(); i++) {
		int err = startPipeline(i);
		if (err)
			return err;
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
	for (int i = 0; i < pipelines.size(); i++)
		dbg->removePipeline(pipelines[i]);
	return BaseLmmElement::stop();
}

int PipelineManager::flush()
{
	for (int i = 0; i < pipelines.size(); i++)
		pipelines[i]->flush();
	return BaseLmmElement::flush();
}

int PipelineManager::getPipelineCount()
{
	return pipelines.size();
}

BaseLmmPipeline *PipelineManager::getPipeline(int ind)
{
	return pipelines[ind];
}

void PipelineManager::timeout()
{
	if (checkPipelineWdts) {
		BaseLmmPipeline *failed = NULL;
		pipelineLock.lock();
		QHashIterator<BaseLmmPipeline *, QElapsedTimer *> hi(pipelineWdts);
		while (hi.hasNext()) {
			hi.next();
			if (hi.key()->getMaxTimeout() && hi.value()->elapsed() > hi.key()->getMaxTimeout()) {
				failed = hi.key();
				break;
			}
		}
		pipelineLock.unlock();
		if (failed) {
			mDebug("pipeline '%s' failed, quitting from application", qPrintable(failed->objectName()));
			QCoreApplication::instance()->exit(234);
		}
	}

	QTimer::singleShot(1000, this, SLOT(timeout()));
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
	pipelineWdts.insert(pipeline, new QElapsedTimer);
	pipelineWdts[pipeline]->start();
	dbg->addPipeline(pipeline);
	connect(pipeline, SIGNAL(playbackFinished()), SLOT(pipelineFinished()));
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

int PipelineManager::startPipeline(int i)
{
	int err = pipelines[i]->start();
	if (err)
		return err;

	QString name = QString("MainPipelineThread%1").arg(i + 1);
	pipelineLock.lock();
	pipelinesByThread.insert(name, pipelines[i]);
	pipelineLock.unlock();
	createOpThread(&PipelineManager::pipelineThread, name, PipelineManager);
	return 0;
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
	if (buf.size())
		currentPipeline->updateStats(buf);
	pipelineLock.lock();
	pipelineWdts[currentPipeline]->restart();
	pipelineLock.unlock();
	return pipelineOutput(currentPipeline, buf);
}
