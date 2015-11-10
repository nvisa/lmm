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

static void elementEventHook(BaseLmmElement *el, const RawBuffer &buf, int ev, void *priv)
{
	PipelineDebugger *dbg = (PipelineDebugger *)priv;
	dbg->elementHook(el, buf, ev);
}

class UdpMessage
{
public:
	UdpMessage() :
		s(&data, QIODevice::WriteOnly)
	{
		initStream();
	}

	UdpMessage(const UdpMessage &other) :
		s(&data, QIODevice::WriteOnly)
	{
		initStream();
		data = other.data;
		valid = other.valid;
		cmd = other.cmd;
		pidx = other.pidx;
		key = other.key;
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
		this->cmd = cmd;
		this->pidx = pidx;
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
	EventData(qint32 cmd, uint timestamp)
	{
		mes = new UdpMessage(cmd, 0);
		mes->s << timestamp;
		t.start();
	}

	int add(ElementIOQueue *q, qint32 bufId, qint32 ev)
	{
		lock.lock();
		mes->s << (qint32)q;
		mes->s << bufId;
		mes->s << ev;
		mes->s << (qint32)t.elapsed();
		msize = mes->s.device()->pos();
		lock.unlock();
		return msize;
	}

	int add(BaseLmmElement *el, qint32 bufId, qint32 ev)
	{
		lock.lock();
		mes->s << (qint32)el;
		mes->s << bufId;
		mes->s << ev;
		mes->s << (qint32)t.elapsed();
		msize = mes->s.device()->pos();
		lock.unlock();
		return msize;
	}

	UdpMessage * finalize(uint timestamp)
	{
		lock.lock();
		UdpMessage *old = mes;
		mes = new UdpMessage(mes->cmd, mes->pidx);
		mes->s << timestamp;
		t.restart();
		lock.unlock();
		return old;
	}

protected:
	QElapsedTimer t;
	UdpMessage *mes;
	QMutex lock;
	int msize;
};

struct PipelineInfo {
	UdpMessage descMes;
	QList<ElementIOQueue *> allQueues;
};

PipelineDebugger::PipelineDebugger(QObject *parent) :
	QObject(parent)
{
	queueEvents = new EventData(CMD_INFO_QUEUE_EVENTS, QDateTime::currentDateTime().toTime_t());
	elementEvents = new EventData(CMD_INFO_ELEMENT_EVENTS, QDateTime::currentDateTime().toTime_t());
	sock = new QUdpSocket(this);
	sock->bind(19000);
	connect(sock, SIGNAL(readyRead()), SLOT(udpDataReady()));
}

void PipelineDebugger::addPipeline(BaseLmmPipeline *pl)
{
	pipelines << pl;
	/*
	 * we don't setup pipeline here, not all queues may
	 * be ready at this point
	 */
}

void PipelineDebugger::queueHook(ElementIOQueue *queue, const RawBuffer &buf, int ev)
{
	if (queueEvents->add(queue, buf.getUniqueId(), ev) > 1400) {
		UdpMessage *mes = queueEvents->finalize(QDateTime::currentDateTime().toTime_t());
		if (!debugPeer.isNull()) {
			sockLock.lock();
			sock->writeDatagram(mes->data, debugPeer, debugPeerPort);
			sockLock.unlock();
		}
		delete mes;
	}
}

void PipelineDebugger::elementHook(BaseLmmElement *el, const RawBuffer &buf, int ev)
{
	if (elementEvents->add(el, buf.getUniqueId(), ev) > 1400) {
		UdpMessage *mes = elementEvents->finalize(QDateTime::currentDateTime().toTime_t());
		if (!debugPeer.isNull()) {
			sockLock.lock();
			sock->writeDatagram(mes->data, debugPeer, debugPeerPort);
			sockLock.unlock();
		}
		delete mes;
	}
}

void PipelineDebugger::udpDataReady()
{
	sockLock.lock();
	while (sock->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(sock->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		sock->readDatagram(datagram.data(), datagram.size(),
								&sender, &senderPort);
		QByteArray ba = processDatagram(datagram);
		if (ba.size()) {
			sock->writeDatagram(ba, sender, senderPort);
			debugPeer = sender;
			debugPeerPort = senderPort;
		}
	}
	sockLock.unlock();
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
	if (!pipelineInfo.contains(in.pidx))
		setupPipelineInfo(in.pidx);
	PipelineInfo *info = pipelineInfo[in.pidx];

	UdpMessage out;
	switch (in.cmd) {
	case CMD_GET_PIPELINE_COUNT:
		out.setupMessageHeader(CMD_INFO_PIPELINE_COUNT, in.pidx);
		out.s << pipelines.size();
		break;
	case CMD_GET_DESCR: {
		out.valid = true;
		out.data = info->descMes.data;
		break;
	}
	case CMD_GET_INFO:
		break;
	case CMD_GET_QUEUE_STATE:
		out.setupMessageHeader(CMD_INFO_QUEUE_STATE, in.pidx);
		out.s << (qint32)info->allQueues.size();
		for (int i = 0; i < info->allQueues.size(); i++) {
			ElementIOQueue *q = info->allQueues[i];
			out.s << (qint32)q;
			out.s << (qint32)q->getBufferCount();
			out.s << (qint32)q->getReceivedCount();
			out.s << (qint32)q->getSentCount();
			out.s << (qint32)q->getFps();
			out.s << (qint32)q->getTotalSize();
		}
		break;
	};

	if (out.valid)
		return out.data;

	return QByteArray();
}

void PipelineDebugger::setupPipelineInfo(int idx)
{
	BaseLmmPipeline *pl = pipelines[idx];
	PipelineInfo *info = new PipelineInfo;
	pipelineInfo.insert(idx, info);
	/* setup pipeline */
	info->descMes.setupMessageHeader(CMD_INFO_DESCR, pipelineInfo.size() - 1);
	int cnt = pl->getPipeCount();
	info->descMes.s << (qint32)cnt;

	for (int i = 0; i < cnt; i++) {
		BaseLmmElement *el = pl->getPipe(i);
		el->setEventHook(elementEventHook, this);
		info->descMes.s << (qint32)el;
		info->descMes.s << el->objectName();
		info->descMes.s << QString(el->metaObject()->className());
		info->descMes.s << el->getInputQueueCount();
		for (int j = 0; j < el->getInputQueueCount(); j++)
			info->descMes.s << (qint32)el->getInputQueue(j);
		info->descMes.s << el->getOutputQueueCount();
		for (int j = 0; j < el->getOutputQueueCount(); j++)
			info->descMes.s << (qint32)el->getOutputQueue(j);
	}

	/* create a unique set of queues */
	QSet<ElementIOQueue *> queues;
	for (int i = 0; i < cnt; i++) {
		BaseLmmElement *el = pl->getPipe(i);
		for (int j = 0; j < el->getInputQueueCount(); j++)
			queues << el->getInputQueue(j);
		for (int j = 0; j < el->getOutputQueueCount(); j++)
			queues << el->getOutputQueue(j);
	}
	info->allQueues = queues.toList();
	for (int i = 0; i < info->allQueues.size(); i++)
		info->allQueues[i]->setEventHook(queueEventHook, this);
}
