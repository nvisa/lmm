#ifndef PIPELINEDEBUGGER_H
#define PIPELINEDEBUGGER_H

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QHostAddress>

#include <lmm/rawbuffer.h>

class EventData;
class QUdpSocket;
class UdpMessage;
class RemotePipeline;
class ElementIOQueue;
class BaseLmmPipeline;
struct PipelineInfo;

class PipelineDebugger : public QObject
{
	Q_OBJECT
public:
	enum Commands {
		CMD_GET_PIPELINE_COUNT,
		CMD_GET_DESCR,
		CMD_GET_INFO,
		CMD_GET_QUEUE_STATE,

		CMD_INFO_PIPELINE_COUNT,
		CMD_INFO_DESCR,
		CMD_INFO_QUEUE_EVENTS,
		CMD_INFO_ELEMENT_EVENTS,
		CMD_INFO_QUEUE_STATE,
	};

	explicit PipelineDebugger(QObject *parent = 0);

	void addPipeline(BaseLmmPipeline *pl);

	void queueHook(ElementIOQueue *queue, const RawBuffer &, int ev, BaseLmmElement *src);
	void elementHook(BaseLmmElement *el, const RawBuffer &buf, int ev);
signals:

public slots:
	void udpDataReady();
	const QByteArray processDatagram(const QByteArray &ba);

protected:
	void setupPipelineInfo(int idx);

	QMutex sockLock;
	QUdpSocket *sock;
	QHostAddress debugPeer;
	quint16 debugPeerPort;
	QList<BaseLmmPipeline *> pipelines;
	EventData *queueEvents;
	EventData *elementEvents;
	QHash<int, PipelineInfo *> pipelineInfo;
};

#endif // PIPELINEDEBUGGER_H
