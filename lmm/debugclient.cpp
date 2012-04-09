#include "debugclient.h"
#include "debugserver.h"

#include <emdesk/debug.h>

#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QDateTime>

DebugClient::DebugClient(QObject *parent) :
	QObject(parent)
{
	client = new QTcpSocket(this);
	connect(client, SIGNAL(connected()), this, SLOT(connectedToServer()));
	connect(client, SIGNAL(disconnected()), this, SLOT(disconnectedFromServer()));
	connect(client, SIGNAL(readyRead()), this, SLOT(readServerMessage()));
	connect(client, SIGNAL(error(QAbstractSocket::SocketError)),
			SLOT(socketError(QAbstractSocket::SocketError)));
}

void DebugClient::connectToServer(QString ipAddr)
{
	client->connectToHost(QHostAddress(ipAddr), DebugServer::serverPortNo());
}

bool DebugClient::isConnected()
{
	if (client->state() == QAbstractSocket::ConnectedState)
		return true;
	return false;
}

void DebugClient::handleMessage(QTcpSocket *client, const QString &cmd,
								QByteArray *data)
{
	QDataStream in(data, QIODevice::ReadOnly);
	if (cmd == "stats") {
		qint32 cnt, props;
		in >> cnt;
		in >> props;
		for (int i = 0; i < cnt; i++) {
			if (stats.size() <= i)
				stats << QList<int>();
			for (int j = 0; j < props; j++) {
				int prop;
				in >> prop;
				if (j >= stats[i].size())
					stats[i] << 0;
				stats[i][j] = prop;
			}
		}
		in >> customStats;
	} else if (cmd == "names") {
		qint32 cnt;
		in >> cnt;
		for (int i = 0; i < cnt; i++) {
			QString name;
			in >> name;
			names << name;
		}
	} else if (cmd == "debugMsg") {
		debugMessages << QString(*data);
	} else if (cmd == "warningMsg") {
		warningMessages << QString(*data);
	}
}

void DebugClient::connectedToServer()
{
	mDebug("%s", qPrintable(client->peerAddress().toString()));
	QTimer::singleShot(100, this, SLOT(timeout()));
	emit connectedToDebugServer();
}

void DebugClient::disconnectedFromServer()
{
	mDebug("disconnected from server, retrying...");
	client->abort();
	emit disconnectedFromDebugServer();
}

void DebugClient::readServerMessage()
{
	QString cmd;
	QByteArray data;
again:
	QDataStream in(client);
	if (blocksize == 0) {
		if ((quint32)client->bytesAvailable() < sizeof(quint32))
			return;
		in >> blocksize;
	}

	if (client->bytesAvailable() < blocksize)
		return;

	in >> cmd;
	in >> data;
	handleMessage(client, cmd, &data);
	blocksize = 0;

	if (client->bytesAvailable())
		goto again;
}

void DebugClient::socketError(QAbstractSocket::SocketError err)
{
	if (err == QAbstractSocket::ConnectionRefusedError) {
		mDebug("socket connection refused to server, retrying after 5 seconds...");
		client->abort();
	}
}

int DebugClient::getInputBufferCount(int el)
{
	if (stats.size() <= el)
		return 0;
	return stats[el][0];
}

int DebugClient::getOutputBufferCount(int el)
{
	if (stats.size() <= el)
		return 0;
	return stats[el][1];
}

int DebugClient::getReceivedBufferCount(int el)
{
	if (stats.size() <= el)
		return 0;
	return stats[el][2];
}

int DebugClient::getSentBufferCount(int el)
{
	if (stats.size() <= el)
		return 0;
	return stats[el][3];
}

int DebugClient::getFps(int el)
{
	if (stats.size() <= el)
		return 0;
	return stats[el][4];
}

int DebugClient::getCustomStat(DebugServer::CustomStat stat)
{
	int s = stat;
	if (customStats.contains(s))
		return customStats.value(s);
	return -1;
}

QStringList DebugClient::getDebugMessages()
{
	QStringList list = debugMessages;
	debugMessages.clear();
	return list;
}

QStringList DebugClient::getWarningMessages()
{
	QStringList list = warningMessages;
	warningMessages.clear();
	return list;
}

int DebugClient::sendCommand(DebugClient::Command cmd, QVariant par)
{
	if (cmd == CMD_SYNC_TIME)
		DebugServer::sendMessage(client, "syncTime",
								 par.toDateTime().toString().toAscii());
	return 0;
}

int DebugClient::getElementCount()
{
	return names.count();
}

const QStringList DebugClient::getElementNames()
{
	return names;
}

QString DebugClient::peerIp()
{
	return client->peerAddress().toString();
}

void DebugClient::timeout()
{
	if (names.size() == 0)
		DebugServer::sendMessage(client, "names");
	else
		DebugServer::sendMessage(client, "stats");
	QTimer::singleShot(100, this, SLOT(timeout()));
}