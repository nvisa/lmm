#include "lmmprocessbus.h"
#include "debug.h"

#include <QEventLoop>
#include <QLocalServer>
#include <QLocalSocket>

#include <errno.h>

#define SERVER_ID				0
#define VERSION_ID_BASE			0x78611494

#define CMD_ID_ASSIGN			0
#define CMD_ID_GET_PNAME		1
#define CMD_ID_LOOKUP_PID		2
#define CMD_ID_LOOKUP_PNAME		3
#define CMD_ID_GET_INT			4
#define CMD_ID_GET_INT_R		5
#define CMD_ID_GET_STRING		6
#define CMD_ID_GET_STRING_R		7
#define CMD_ID_GET_VARIANT		8
#define CMD_ID_GET_VARIANT_R	9
#define CMD_ID_SET_INT			10
#define CMD_ID_SET_INT_R		11
#define CMD_ID_SET_STRING		12
#define CMD_ID_SET_VARIANT		14

#define messageDataStream(__x, __y, __z) \
	QDataStream out(&ba, QIODevice::WriteOnly); \
	out.setByteOrder(QDataStream::LittleEndian); \
	out.setFloatingPointPrecision(QDataStream::SinglePrecision); \
	out << (qint32)VERSION_ID_BASE; \
	out << myid; \
	out << (qint32)0; \
	out << __x; \
	out << __y; \
	out << __z;

#define messageSetSize() \
	qint32 len = out.device()->pos(); \
	out.device()->seek(8); \
	out << len

static void dump(const QByteArray &ba)
{
	for (int i = 0; i < ba.size(); i++)
		qDebug("0x%x: 0x%x", i, ba.at(i));
}

LmmProcessBus::LmmProcessBus(LmmPBusInterface *interface, QObject *parent) :
	QObject(parent),
	iface(interface)
{
	idWait = NULL;
	myid = -1;
	csock = NULL;
}

int LmmProcessBus::join()
{
	csock = new QLocalSocket(this);
	connect(csock, SIGNAL(connected()), SLOT(connectedToServer()));
	connect(csock, SIGNAL(disconnected()), SLOT(sockDisconnected()));
	connect(csock, SIGNAL(readyRead()), SLOT(sockDataReady()));
	csock->connectToServer("/tmp/pbus");
	return 0;
}

int LmmProcessBus::waitForConnected()
{
	if (csock->state() == QLocalSocket::ConnectedState)
		return 0;
	if (csock->waitForConnected())
		return 0;
	return -ETIMEDOUT;
}

int LmmProcessBus::waitForID()
{
	if (myid > 0)
		return 0;
	QEventLoop el;
	idWait = &el;
	mDebug("waiting for id");
	el.exec();
	idWait = NULL;
	return 0;
}

int LmmProcessBus::lookUpId(const QString &pname)
{
	qint32 uid = rand();
	csock->write(createStringMessage(CMD_ID_LOOKUP_PID, SERVER_ID, pname, uid));
	waitMes(uid);
	int val = responseInt[uid];
	responseInt.remove(uid);
	return val;
}

int LmmProcessBus::getInt(qint32 target, const QString &fld)
{
	qint32 uid = rand();
	csock->write(createStringMessage(CMD_ID_GET_INT, target, fld, uid));
	waitMes(uid);
	int val = responseInt[uid];
	responseInt.remove(uid);
	return val;
}

int LmmProcessBus::setInt(qint32 target, const QString &fld, qint32 val)
{
	qint32 uid = rand();
	csock->write(createStringMessage1(CMD_ID_SET_INT, target, fld, val, uid));
	waitMes(uid);
	val = responseInt[uid];
	responseInt.remove(uid);
	return val;
}

QString LmmProcessBus::getString(qint32 target, const QString &fld)
{
	qint32 uid = rand();
	csock->write(createStringMessage(CMD_ID_GET_STRING, target, fld, uid));
	waitMes(uid);
	QString val = responseString[uid];
	responseString.remove(uid);
	return val;
}

int LmmProcessBus::setString(qint32 target, const QString &fld, const QString &val)
{
	qint32 uid = rand();
	csock->write(createStringMessage1(CMD_ID_SET_INT, target, fld, val, uid));
	waitMes(uid);
	int err = responseInt[uid];
	responseInt.remove(uid);
	return err;
}

QVariant LmmProcessBus::getVariant(qint32 target, const QString &fld)
{
	qint32 uid = rand();
	csock->write(createStringMessage(CMD_ID_GET_VARIANT, target, fld, uid));
	waitMes(uid);
	QVariant val = responseVariant[uid];
	responseVariant.remove(uid);
	return val;
}

int LmmProcessBus::setVariant(qint32 target, const QString &fld, const QVariant &val)
{
	qint32 uid = rand();
	csock->write(createStringMessage1(CMD_ID_SET_INT, target, fld, val, uid));
	waitMes(uid);
	int err = responseInt[uid];
	responseInt.remove(uid);
	return err;
}

void LmmProcessBus::sockDataReady()
{
	QLocalSocket *sock = (QLocalSocket *)sender();
	QByteArray ba = sock->readAll();
	QDataStream in(ba);
	in.setByteOrder(QDataStream::LittleEndian);
	in.setFloatingPointPrecision(QDataStream::SinglePrecision);
	processMessage(sock, in);
}

void LmmProcessBus::sockDisconnected()
{
	QLocalSocket *sock = (QLocalSocket *)sender();
	sock->deleteLater();
	if (sock == csock) {
		mDebug("disconnected from server");
		csock = NULL;
	}
}

void LmmProcessBus::connectedToServer()
{
	mDebug("connected to server");
	emit joined();
}

QByteArray LmmProcessBus::createIntMessage(qint32 cmd, qint32 target, qint32 mes, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	messageSetSize();
	return ba;
}

QByteArray LmmProcessBus::createStringMessage(qint32 cmd, qint32 target, const QString &mes, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	messageSetSize();
	return ba;
}

QByteArray LmmProcessBus::createStringMessage1(qint32 cmd, qint32 target, const QString &mes, const QString &val1, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	out << val1;
	messageSetSize();
	return ba;
}

QByteArray LmmProcessBus::createStringMessage1(qint32 cmd, qint32 target, const QString &mes, const QVariant &val1, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	out << val1;
	messageSetSize();
	return ba;
}

QByteArray LmmProcessBus::createStringMessage1(qint32 cmd, qint32 target, const QString &mes, qint32 val1, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	out << val1;
	messageSetSize();
	return ba;
}

QByteArray LmmProcessBus::createVariantMessage(qint32 cmd, qint32 target, const QVariant &mes, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	messageSetSize();
	return ba;
}

QByteArray LmmProcessBus::createCommandMessage(qint32 cmd, qint32 target, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	messageSetSize();
	return ba;
}

QByteArray LmmProcessBus::processMessage(QDataStream &in, QLocalSocket *sock, const MessageData &msg)
{
	Q_UNUSED(sock);
	if (msg.sender == SERVER_ID && msg.cmd == CMD_ID_ASSIGN) {
		myid = msg.target;
		mDebug("got my id %d", myid);
		if (idWait)
			idWait->quit();
		return QByteArray();
	}
	if (msg.target != myid) {
		mInfo("non-target message: target=%d myid=%d", msg.target, myid);
		in.skipRawData(msg.len - 24);
		return QByteArray();
	}
	if (msg.cmd == CMD_ID_GET_PNAME)
		return createStringMessage(CMD_ID_GET_PNAME, SERVER_ID, iface->getProcessName());

	return processMessageCommon(in, msg.sender, msg.cmd, msg.uid);
}

QByteArray LmmProcessBus::processMessageCommon(QDataStream &in, qint32 sender, qint32 cmd, qint32 uid)
{
	mInfo("new message: cmd=%d uid=%d", cmd, uid);
	if (cmd == CMD_ID_GET_INT ||
			cmd == CMD_ID_GET_STRING ||
			cmd == CMD_ID_GET_VARIANT
			) {
		QString fld; in >> fld;
		if (cmd == CMD_ID_GET_INT)
			return createIntMessage(CMD_ID_GET_INT_R, sender, iface->getInt(fld), uid);
		if (cmd == CMD_ID_GET_STRING)
			return createStringMessage(CMD_ID_GET_STRING_R, sender, iface->getString(fld), uid);
		if (cmd == CMD_ID_GET_VARIANT)
			return createVariantMessage(CMD_ID_GET_VARIANT_R, sender, iface->getVariant(fld), uid);
	} else if (cmd == CMD_ID_SET_INT ||
			   cmd == CMD_ID_SET_STRING ||
			   cmd == CMD_ID_SET_VARIANT) {
		QString fld; in >> fld;
		if (cmd == CMD_ID_SET_INT) {
			qint32 val; in >> val;
			return createIntMessage(CMD_ID_SET_INT_R, sender, iface->setInt(fld, val), uid);
		}
		if (cmd == CMD_ID_SET_STRING) {
			QString val; in >> val;
			return createIntMessage(CMD_ID_SET_INT_R, sender, iface->setString(fld, val), uid);
		}
		if (cmd == CMD_ID_SET_VARIANT) {
			QVariant val; in >> val;
			return createIntMessage(CMD_ID_SET_INT_R, sender, iface->setVariant(fld, val), uid);
		}
	} else if (cmd == CMD_ID_GET_INT_R || cmd == CMD_ID_LOOKUP_PID || cmd == CMD_ID_SET_INT_R) {
		qint32 val; in >> val;
		responseInt[uid] = val;
		notifyWaiters(uid);
	} else if (cmd == CMD_ID_GET_STRING_R) {
		QString val; in >> val;
		responseString[uid] = val;
		notifyWaiters(uid);
	} else if (cmd == CMD_ID_GET_VARIANT_R) {
		QVariant val; in >> val;
		responseVariant[uid] = val;
		notifyWaiters(uid);
	}
	return QByteArray();
}

void LmmProcessBus::processMessage(QLocalSocket *sock, QDataStream &in)
{
	MessageData msg;
	in >> msg.version;
	if (msg.version != VERSION_ID_BASE) {
		mDebug("message version mismatch!");
		return;
	}
	in >> msg.sender;
	in >> msg.len;
	in >> msg.cmd;
	in >> msg.target;
	in >> msg.uid;

	QByteArray resp = processMessage(in, sock, msg);
	if (resp.size())
		sock->write(resp);

	/* we may have multiple messages */
	while (!in.atEnd())
		processMessage(sock, in);
}

void LmmProcessBus::notifyWaiters(qint32 uid)
{
	if (waiters.contains(uid)) {
		waiters[uid]->quit();
		waiters.remove(uid);
	}
}

bool LmmProcessBus::waitMes(qint32 uid)
{
	QEventLoop el;
	waiters.insert(uid, &el);
	mInfo("%d", uid);
	el.exec();
	return true;
}

LmmProcessBusServer::LmmProcessBusServer(LmmPBusInterface *interface, QObject *parent)
	: LmmProcessBus(interface, parent)
{
	bus = NULL;
	for (int i = 1; i < 1000; i++)
		uniqueIdList << i;
	srand(time(NULL));
}

LmmProcessBusImpl::LmmProcessBusImpl(QObject *parent)
	: QObject(parent)
{
	server = NULL;
}

int LmmProcessBusServer::setupBus()
{
	myid = SERVER_ID;
	bus = new LmmProcessBusImpl(this);
	connect(bus, SIGNAL(clientConnected(sockType)), SLOT(clientConnectedToBus(sockType)));
	connect(bus, SIGNAL(clientDisconnected(sockType)), SLOT(clientDisconnectedFromBus(sockType)));
	return bus->setup();
}

QStringList LmmProcessBusServer::getConnectedProcesses()
{
	return processNames.values();
}

void LmmProcessBusServer::clientConnectedToBus(sockType val)
{
	mDebug("new client connected");

	/* check unique id availibility */
	if (uniqueIdList.size() < 10) {
		int m = uniqueIdList.last();
		for (int i = 1; i < 1000; i++)
			uniqueIdList << m + i;
	}

	if (processIds.size() == 0) {
		/* this is me joining the bus */
		processIds.insert(val, SERVER_ID);
		return;
	}
	int target = uniqueIdList.takeFirst();
	processIds.insert(val, target);
	QLocalSocket *sock = (QLocalSocket *)val;
	sock->write(createCommandMessage(CMD_ID_ASSIGN, target));
	sock->write(createCommandMessage(CMD_ID_GET_PNAME, target));
}

void LmmProcessBusServer::clientDisconnectedFromBus(sockType val)
{
	qint32 pid = processIds[val];
	processNames.remove(pid);
}

QByteArray LmmProcessBusServer::processMessage(QDataStream &in, QLocalSocket *sock, const LmmProcessBus::MessageData &msg)
{
	Q_UNUSED(sock);
	if (msg.target != myid) {
		if (!processNames.contains(msg.target))
			mDebug("un-expected message id for cmd %d: target=%d myid=%d", msg.cmd, msg.target, myid);
		in.skipRawData(msg.len - 24);
		return QByteArray();
	}
	if (msg.cmd == CMD_ID_GET_PNAME) {
		QString pname; in >> pname;
		processNames.insert(msg.sender, pname);
		return QByteArray();
	} else if (msg.cmd == CMD_ID_LOOKUP_PID) {
		QString pname; in >> pname;
		int pid = -1;
		QHashIterator<int, QString> i(processNames);
		while (i.hasNext()) {
			i.next();
			if (i.value() == pname) {
				pid = i.key();
				break;
			}
		}
		mDebug("looked-up %s: %d", qPrintable(pname), pid);
		return createIntMessage(CMD_ID_LOOKUP_PID, msg.sender, pid, msg.uid);
	} else if (msg.cmd == CMD_ID_GET_PNAME) {
		qint32 pid; in >> pid;
		return createStringMessage(CMD_ID_LOOKUP_PID, msg.sender, processNames[pid]);
	}

	return processMessageCommon(in, msg.sender, msg.cmd, msg.uid);
}

int LmmProcessBusImpl::setup()
{
	server = new QLocalServer(this);
	connect(server, SIGNAL(newConnection()), SLOT(newConnection()));
	if (server->listen("/tmp/pbus"))
		return 0;
	return -EPERM;
}

void LmmProcessBusImpl::dataReady()
{
	QLocalSocket *sock = (QLocalSocket *)sender();
	QByteArray ba = sock->readAll();
	for (int i = 0; i < connections.size(); i++) {
		QLocalSocket *s = connections[i];
		if (s != sock)
			s->write(ba);
	}
}

void LmmProcessBusImpl::newConnection()
{
	QLocalSocket *sock = server->nextPendingConnection();
	connect(sock, SIGNAL(disconnected()), SLOT(sockDisconnected()));
	connect(sock, SIGNAL(readyRead()), SLOT(dataReady()));
	connections << sock;
	emit clientConnected((sockType)sock);
}

void LmmProcessBusImpl::sockDisconnected()
{
	QLocalSocket *sock = (QLocalSocket *)sender();
	connections.removeAll(sock);
	sock->deleteLater();
	emit clientDisconnected((sockType)sock);
}

int LmmProcessBusImpl::getProcessCount()
{
	return connections.size();
}
