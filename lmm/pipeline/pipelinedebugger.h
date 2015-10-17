#ifndef PIPELINEDEBUGGER_H
#define PIPELINEDEBUGGER_H

#include <QHash>
#include <QMutex>
#include <QObject>

#include <lmm/rawbuffer.h>

class EventData;
class QUdpSocket;
class UdpMessage;
class RemotePipeline;
class ElementIOQueue;
class BaseLmmPipeline;

class PipelineDebugger : public QObject
{
	Q_OBJECT
public:
	enum Commands {
		CMD_GET_PIPELINE_COUNT,
		CMD_GET_DESCR,
		CMD_GET_INFO,

		CMD_INFO_PIPELINE_COUNT,
		CMD_INFO_DESCR,
		CMD_INFO_QUEUE_EVENTS
	};

	explicit PipelineDebugger(QObject *parent = 0);

	void addPipeline(BaseLmmPipeline *pipeline);

	void queueHook(ElementIOQueue *queue, const RawBuffer &, int ev);
signals:

public slots:
	void udpDataReady();
	const QByteArray processDatagram(const QByteArray &ba);

protected:
	QUdpSocket *sock;
	QList<BaseLmmPipeline *> pipelines;
	QList<ElementIOQueue *> allQueues;
	EventData *queueEvents;
};

#endif // PIPELINEDEBUGGER_H
