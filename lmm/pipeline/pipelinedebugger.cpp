#include "pipelinedebugger.h"
#include "baselmmpipeline.h"
#include "debug.h"

#include <QUdpSocket>
#include <QDataStream>

static void queueEventHook(ElementIOQueue *queue, const RawBuffer &buf, int ev, void *priv)
{
	PipelineDebugger *dbg = (PipelineDebugger *)priv;
	dbg->queueHook(queue, buf, ev);
}

class UdpMessage
{
public:
	UdpMessage() :
		s(&data, QIODevice::WriteOnly)
	{
		initStream();
	}

	UdpMessage(qint32 cmd, qint32 pidx) :
		s(&data, QIODevice::WriteOnly)
	{
		initStream();
		setupMessageHeader(cmd, pidx);
	}

	UdpMessage(const QByteArray &ba) :
		s(ba)
	{
		initStream();
		readHeader();
	}

	void initStream()
	{
		s.setFloatingPointPrecision(QDataStream::SinglePrecision);
		s.setByteOrder(QDataStream::LittleEndian);
		valid = false;
	}

	void readHeader()
	{
		s >> key;
		if (key != 0x14757896)
			return;
		valid = true;
		s >> cmd;
		s >> pidx;
	}

	void setupMessageHeader(qint32 cmd, qint32 pidx)
	{
		s << (qint32)0x14757896;
		s << cmd;
		s << pidx;
		valid = true;
	}

	QByteArray data;
	QDataStream s;
	bool valid;
	qint32 cmd;
	qint32 pidx;
	qint32 key;
};

class EventData
{
public:
	EventData(qint32 cmd)
	{
		mes = new UdpMessage(cmd, 0);
	}

	void add(ElementIOQueue *q, qint32 bufId, qint32 ev)
	{
		lock.lock();
		mes->s << (qint32)q;
		mes->s << bufId;
		mes->s << ev;
		lock.unlock();
	}

protected:
	UdpMessage *mes;
	QMutex lock;
};

PipelineDebugger::PipelineDebugger(QObject *parent) :
	QObject(parent)
{
	queueEvents = new EventData(CMD_INFO_QUEUE_EVENTS);
	sock = new QUdpSocket(this);
	sock->bind(19000);
	connect(sock, SIGNAL(readyRead()), SLOT(udpDataReady()));
}

void PipelineDebugger::addPipeline(BaseLmmPipeline *pipeline)
{
	pipelines << pipeline;
}

void PipelineDebugger::queueHook(ElementIOQueue *queue, const RawBuffer &buf, int ev)
{
	queueEvents->add(queue, buf.getUniqueId(), ev);
}

void PipelineDebugger::udpDataReady()
{
	while (sock->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(sock->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		sock->readDatagram(datagram.data(), datagram.size(),
								&sender, &senderPort);
		QByteArray ba = processDatagram(datagram);
		if (ba.size())
			sock->writeDatagram(ba, sender, senderPort);
	}
}

const QByteArray PipelineDebugger::processDatagram(const QByteArray &ba)
{
	UdpMessage in(ba);
	if (!in.valid)
		return QByteArray();
	if (in.pidx >= pipelines.size())
		return QByteArray();
	BaseLmmPipeline *pl = pipelines[in.pidx];
	ffDebug() << in.cmd << in.pidx << pl;

	UdpMessage out;
	switch (in.cmd) {
	case CMD_GET_PIPELINE_COUNT:
		out.setupMessageHeader(CMD_INFO_PIPELINE_COUNT, in.pidx);
		out.s << pipelines.size();
		break;
	case CMD_GET_DESCR: {
		out.setupMessageHeader(CMD_INFO_DESCR, in.pidx);

		int cnt = pl->getPipeCount();
		out.s << (qint32)cnt;

		for (int i = 0; i < cnt; i++) {
			BaseLmmElement *el = pl->getPipe(i);
			out.s << el->objectName();
			out.s << QString(el->metaObject()->className());
			out.s << el->getInputQueueCount();
			for (int j = 0; j < el->getInputQueueCount(); j++)
				out.s << (qint32)el->getInputQueue(j);
			out.s << el->getOutputQueueCount();
			for (int j = 0; j < el->getOutputQueueCount(); j++)
				out.s << (qint32)el->getOutputQueue(j);
		}

		QSet<ElementIOQueue *> queues;
		for (int i = 0; i < cnt; i++) {
			BaseLmmElement *el = pl->getPipe(i);
			for (int j = 0; j < el->getInputQueueCount(); j++)
				queues << el->getInputQueue(j);
			for (int j = 0; j < el->getOutputQueueCount(); j++)
				queues << el->getOutputQueue(j);
		}
		allQueues = queues.toList();
		for (int i = 0; i < allQueues.size(); i++)
			allQueues[i]->setEventHook(queueEventHook, this);
		break;
	}
	case CMD_GET_INFO:
		break;
	};

	if (out.valid)
		return out.data;

	return QByteArray();
}
