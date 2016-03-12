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
#define CMD_ID_SET_STRING_R		13
#define CMD_ID_SET_VARIANT		14
#define CMD_ID_SET_VARIANT_R	15

#define messageDataStream(__x, __y, __z) \
	QDataStream out(&ba, QIODevice::WriteOnly); \
	out.setByteOrder(QDataStream::LittleEndian); \
	out.setFloatingPointPrecision(QDataStream::SinglePrecision); \
	out << (qint32)VERSION_ID_BASE; \
	out << myid; \
	out << __x; \
	out << __y; \
	out << __z; \

LmmProcessBus::LmmProcessBus(LmmPBusInterface *interface, QObject *parent) :
	QObject(parent),
	iface(interface)
{
	idWait = NULL;
	myid = -1;
	server = NULL;
	csock = NULL;
	for (int i = 1; i < 1000; i++)
		uniqueIdList << i;
	srand(time(NULL));
}

int LmmProcessBus::setupBus()
{
	myid = SERVER_ID;
	server = new QLocalServer(this);
	connect(server, SIGNAL(newConnection()), SLOT(newConnection()));
	if (server->listen("/tmp/pbus"))
		return 0;
	return -EPERM;
}

QStringList LmmProcessBus::getConnectedProcesses()
{
	return processNames.values();
}

int LmmProcessBus::getProcessCount()
{
	return connections.size();
}

int LmmProcessBus::join()
{
	csock = new QLocalSocket(this);
	connections << csock;
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

void LmmProcessBus::newConnection()
{
	QLocalSocket *sock = server->nextPendingConnection();
	connect(sock, SIGNAL(disconnected()), SLOT(sockDisconnected()));
	connect(sock, SIGNAL(readyRead()), SLOT(sockDataReady()));
	connections << sock;
	mDebug("new client connected");

	/* check unique id availibility */
	if (uniqueIdList.size() < 10) {
		int m = uniqueIdList.last();
		for (int i = 1; i < 1000; i++)
			uniqueIdList << m + i;
	}

	int target = uniqueIdList.takeFirst();
	sock->write(createCommandMessage(CMD_ID_ASSIGN, target));
	sock->write(createCommandMessage(CMD_ID_GET_PNAME, target));
}

void LmmProcessBus::sockDataReady()
{
	QLocalSocket *sock = (QLocalSocket *)sender();
	QByteArray ba = sock->readAll();
	QDataStream in(ba);
	in.setByteOrder(QDataStream::LittleEndian);
	in.setFloatingPointPrecision(QDataStream::SinglePrecision);
	processMessage(sock, in);
	for (int i = 0; i < connections.size(); i++) {
		QLocalSocket *s = connections[i];
		if (s != sock)
			s->write(ba);
	}
}

void LmmProcessBus::sockDisconnected()
{
	QLocalSocket *sock = (QLocalSocket *)sender();
	connections.removeAll(sock);
	qint32 pid = processIds[sock];
	processNames.remove(pid);
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

void LmmProcessBus::sockError(int err)
{
	Q_UNUSED(err);
}

QByteArray LmmProcessBus::createIntMessage(qint32 cmd, qint32 target, qint32 mes, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	return ba;
}

QByteArray LmmProcessBus::createStringMessage(qint32 cmd, qint32 target, const QString &mes, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	return ba;
}

QByteArray LmmProcessBus::createStringMessage1(qint32 cmd, qint32 target, const QString &mes, const QString &val1, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	out << val1;
	return ba;
}

QByteArray LmmProcessBus::createStringMessage1(qint32 cmd, qint32 target, const QString &mes, const QVariant &val1, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	out << val1;
	return ba;
}

QByteArray LmmProcessBus::createStringMessage1(qint32 cmd, qint32 target, const QString &mes, qint32 val1, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	out << val1;
	return ba;
}

QByteArray LmmProcessBus::createVariantMessage(qint32 cmd, qint32 target, const QVariant &mes, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	out << mes;
	return ba;
}

QByteArray LmmProcessBus::createCommandMessage(qint32 cmd, qint32 target, qint32 uid)
{
	QByteArray ba;
	messageDataStream(cmd, target, uid);
	return ba;
}

QByteArray LmmProcessBus::processMessageClient(QDataStream &in, QLocalSocket *sock, qint32 sender, qint32 cmd, qint32 target, qint32 uid)
{
	Q_UNUSED(sock);
	if (sender == SERVER_ID && cmd == CMD_ID_ASSIGN) {
		myid = target;
		mDebug("got my id %d", myid);
		if (idWait)
			idWait->quit();
		return QByteArray();
	}
	if (target != myid) {
		mDebug("un-expected message id");
		return QByteArray();
	}
	if (cmd == CMD_ID_GET_PNAME)
		return createStringMessage(CMD_ID_GET_PNAME, SERVER_ID, iface->getProcessName());

	return processMessageCommon(in, sender, cmd, uid);
}

QByteArray LmmProcessBus::processMessageServer(QDataStream &in, QLocalSocket *sock, qint32 sender, qint32 cmd, qint32 target, qint32 uid)
{
	if (target != myid) {
		if (!processNames.contains(target))
			mDebug("un-expected message id for cmd %d: target=%d myid=%d", cmd, target, myid);
		return QByteArray();
	}
	if (cmd == CMD_ID_GET_PNAME) {
		QString pname; in >> pname;
		processIds.insert(sock, sender);
		processNames.insert(sender, pname);
		return QByteArray();
	} else if (cmd == CMD_ID_LOOKUP_PID) {
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
		return createIntMessage(CMD_ID_LOOKUP_PID, sender, pid, uid);
	} else if (cmd == CMD_ID_GET_PNAME) {
		qint32 pid; in >> pid;
		return createStringMessage(CMD_ID_LOOKUP_PID, sender, processNames[pid]);
	}

	return processMessageCommon(in, sender, cmd, uid);
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
	} else if (cmd == CMD_ID_GET_INT_R || cmd == CMD_ID_LOOKUP_PID) {
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
	qint32 version; in >> version;
	if (version != VERSION_ID_BASE) {
		mDebug("message version mismatch!");
		return;
	}
	qint32 sender; in >> sender;
	qint32 cmd; in >> cmd;
	qint32 target; in >> target;
	qint32 uid; in >> uid;

	QByteArray resp;
	if (server)
		resp = processMessageServer(in, sock, sender, cmd, target, uid);
	else
		resp = processMessageClient(in, sock, sender, cmd, target, uid);
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
