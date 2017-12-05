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

#if QT_VERSION > 0x050000
typedef BaseLmmElement * (*elementCreateFactory)(const QJsonObject &elobj);
#endif

class PipelineManager : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit PipelineManager(QObject *parent = 0);

	virtual int start();
	virtual int stop();
	virtual int flush();

	int getPipelineCount();
	BaseLmmPipeline * getPipeline(int ind);

	int parseConfig(const QString &filename);
#if QT_VERSION > 0x050000
	static void registerElementFactory(const QString &type, elementCreateFactory factory);
#endif

signals:

protected slots:
	virtual void timeout();
	virtual void pipelineFinished() {}
protected:
	virtual void signalReceived(int sig);
	virtual int processBuffer(const RawBuffer &buf);
	BaseLmmPipeline * addPipeline();
	virtual int pipelineOutput(BaseLmmPipeline *p, const RawBuffer &buf);
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
	int debug;
};

#endif // PIPELINEMANAGER_H
