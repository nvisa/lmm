#ifndef PIPELINEMANAGER_H
#define PIPELINEMANAGER_H

#include <QHash>
#include <QMutex>
#include <QObject>

#include <lmm/rawbuffer.h>
#include <lmm/baselmmelement.h>

class LmmThread;
class BaseLmmPipeline;
class PipelineDebugger;

class PipelineManager : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit PipelineManager(QObject *parent = 0);

	virtual int start();
	virtual int stop();
signals:

public slots:
protected:
	virtual void signalReceived(int sig);
	virtual int processBuffer(const RawBuffer &buf);
	BaseLmmPipeline * addPipeline();
	virtual int pipelineOutput(BaseLmmPipeline *, const RawBuffer &) { return 0; }

private:
	int pipelineThread();

	PipelineDebugger *dbg;
	QList<BaseLmmPipeline *> pipelines;
	QHash<QString, LmmThread *> threads;
	QHash<QString, BaseLmmPipeline *> pipelinesByThread;
	QMutex pipelineLock;
	bool quitting;
};

#endif // PIPELINEMANAGER_H
