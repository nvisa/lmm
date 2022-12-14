#include "pipelinedebugger.h"
#include "baselmmpipeline.h"
#include "debug.h"

#include <QThread>
#include <QDateTime>
#include <QUdpSocket>
#include <QDataStream>
#include <QElapsedTimer>

#include <stdint.h>

PipelineDebugger * PipelineDebugger::inst = NULL;

#ifdef HAVE_GRPC
#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/channel.h>
#include <grpc++/create_channel.h>
#include <grpc++/client_context.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/credentials.h>

#include <grpc++/security/server_credentials.h>

#include "proto/pipeline.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::Status;
using namespace std;

class GRpcServerImpl : public Lmm::PipelineService::Service
{
public:
	GRpcServerImpl(PipelineDebugger *dbg)
	{
		pd = dbg;
	}

	virtual grpc::Status GetInfo(grpc::ServerContext *, const Lmm::GenericQ *, Lmm::PipelinesInfo *response)
	{
		for (int i = 0; i < pd->getPipelineCount(); i++) {
			BaseLmmPipeline *lmmp = pd->getPipeline(i);

			Lmm::Pipeline *p = response->add_pipelines();
			p->set_name(qPrintable(lmmp->objectName()));
			for (int j = 0; j < lmmp->getPipeCount(); j++) {
				BaseLmmElement *lmmel = lmmp->getPipe(j);
				Lmm::Element *el = p->add_elements();
				el->set_name(qPrintable(lmmel->objectName()));

				for (int k = 0; k < lmmel->getInputQueueCount(); k++) {
					ElementIOQueue *lmmq = lmmel->getInputQueue(k);
					Lmm::QueueInfo *q = el->add_inq();
					q->set_buffercount(lmmq->getBufferCount());
				}

				for (int k = 0; k < lmmel->getOutputQueueCount(); k++) {
					ElementIOQueue *lmmq = lmmel->getOutputQueue(k);
					Lmm::QueueInfo *q = el->add_outq();
					q->set_buffercount(lmmq->getBufferCount());
				}
			}
		}

		return Status::OK;
	}

	virtual grpc::Status GetQueueInfo(grpc::ServerContext *, const Lmm::QueueInfoQ *request, Lmm::QueueInfo *response)
	{
		int pi = request->pipeline();
		int ei = request->element();
		int inqi = request->inqi();
		int outi = request->outqi();
		ElementIOQueue *q = NULL;
		if (inqi >= 0)
			q = pd->getPipeline(pi)->getPipe(ei)->getInputQueue(inqi);
		else if (outi >= 0)
			q = pd->getPipeline(pi)->getPipe(ei)->getOutputQueue(outi);
		response->set_buffercount(0);
		if (!q)
			return Status::OK;

		response->set_buffercount(q->getBufferCount());
		response->set_elapsedsincelastadd(q->getElapsedSinceLastAdd());
		response->set_fps(q->getFps());
		response->set_receivedcount(q->getReceivedCount());
		response->set_sentcount(q->getSentCount());
		response->set_totalsize(q->getTotalSize());
		response->set_bitrate(q->getBitrate());
		response->set_ratelimit((Lmm::QueueInfo_RateLimit)q->getRateLimit());

		return Status::OK;
	}

protected:
	PipelineDebugger *pd;

protected:
};

class GrpcThread : public QThread
{
public:
	GrpcThread(quint16 port, GRpcServerImpl *s)
	{
		servicePort = port;
		service = s;
	}

	void run()
	{
		string ep(qPrintable(QString("0.0.0.0:%1").arg(servicePort)));
		ServerBuilder builder;
		builder.AddListeningPort(ep, grpc::InsecureServerCredentials());
		builder.RegisterService(service);
		std::unique_ptr<Server> server(builder.BuildAndStart());
		server->Wait();
	}
protected:
	int servicePort;
	GRpcServerImpl *service;
};

#endif

static void queueEventHook(ElementIOQueue *queue, const RawBuffer &buf, int ev, BaseLmmElement *src, void *priv)
{
	PipelineDebugger *dbg = (PipelineDebugger *)priv;
	dbg->queueHook(queue, buf, ev, src);
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

	int add(ElementIOQueue *q, qint32 bufId, qint32 ev, BaseLmmElement *src)
	{
		lock.lock();
		mes->s << (qint64)q;
		mes->s << bufId;
		mes->s << ev;
		mes->s << (qint64)src;
		mes->s << (qint32)t.elapsed();
		msize = mes->s.device()->pos();
		lock.unlock();
		return msize;
	}

	int add(BaseLmmElement *el, qint32 bufId, qint32 ev)
	{
		lock.lock();
		mes->s << (qint64)el;
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
	started = false;
}

PipelineDebugger *PipelineDebugger::GetInstance()
{
	if (!inst)
		inst = new PipelineDebugger;
	return inst;
}

void PipelineDebugger::addPipeline(BaseLmmPipeline *pl)
{
	pipelines << pl;
	/*
	 * we don't setup pipeline here, not all queues may
	 * be ready at this point
	 */
}

int PipelineDebugger::getPipelineCount()
{
	return pipelines.size();
}

BaseLmmPipeline *PipelineDebugger::getPipeline(int ind)
{
	return pipelines[ind];
}

void PipelineDebugger::removePipeline(BaseLmmPipeline *pl)
{
	pipelines.removeAll(pl);
}

int PipelineDebugger::start()
{
	connect(sock, SIGNAL(readyRead()), SLOT(udpDataReady()));

#ifdef HAVE_GRPC
	GrpcThread *thr = new GrpcThread(19101, new GRpcServerImpl(this));
	thr->start();
#endif

	started = true;

	return 0;
}

bool PipelineDebugger::isStarted()
{
	return started;
}

void PipelineDebugger::queueHook(ElementIOQueue *queue, const RawBuffer &buf, int ev, BaseLmmElement *src)
{
	if (queueEvents->add(queue, buf.getUniqueId(), ev, src) > 1400) {
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
			out.s << (qint64)q;
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
		info->descMes.s << (qint64)el;
		info->descMes.s << el->objectName();
		info->descMes.s << QString(el->metaObject()->className());
		info->descMes.s << el->getInputQueueCount();
		for (int j = 0; j < el->getInputQueueCount(); j++)
			info->descMes.s << (qint64)el->getInputQueue(j);
		info->descMes.s << el->getOutputQueueCount();
		for (int j = 0; j < el->getOutputQueueCount(); j++)
			info->descMes.s << (qint64)el->getOutputQueue(j);
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
