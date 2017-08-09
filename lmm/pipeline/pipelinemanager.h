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

	int getPipelineCount();
	BaseLmmPipeline * getPipeline(int ind);
signals:

protected slots:
	virtual void timeout();
	virtual void pipelineFinished() {}
protected:
	virtual void signalReceived(int sig);
	virtual int processBuffer(const RawBuffer &buf);
	BaseLmmPipeline * addPipeline();
	virtual int pipelineOutput(BaseLmmPipeline *, const RawBuffer &) { return 0; }
	QList<BaseLmmElement *> getElements();
	int startPipeline(int i);

	QMutex pipelineLock;
	bool checkPipelineWdts;
private:
	int pipelineThread();

	PipelineDebugger *dbg;
	QList<BaseLmmPipeline *> pipelines;
	QHash<QString, LmmThread *> threads;
	QHash<QString, BaseLmmPipeline *> pipelinesByThread;
	QHash<BaseLmmPipeline *, QElapsedTimer *> pipelineWdts;
	bool quitting;
};

#endif // PIPELINEMANAGER_H
