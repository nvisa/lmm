#include "debugserver.h"
#include "baselmmelement.h"

#include <emdesk/debug.h>

#include <QTcpServer>
#include <QTcpSocket>

static DebugServer *debugServer = NULL;

static void serverMessageOutput(QtMsgType type, const char *msg)
{
	switch (type) {
	case QtDebugMsg:
		debugServer->sendMessage("debugMsg", msg);
		//fprintf(stderr, "Debug: %s\n", msg);
		break;
	case QtWarningMsg:
		debugServer->sendMessage("warningMsg", msg);
		//fprintf(stderr, "Warning: %s\n", msg);
		break;
	case QtCriticalMsg:
		debugServer->sendMessage("criticalMsg", msg);
		//fprintf(stderr, "Critical: %s\n", msg);
		break;
	case QtFatalMsg:
		debugServer->sendMessage("fatalMsg", msg);
		//fprintf(stderr, "Fatal: %s\n", msg);
		//abort();
	}
}

DebugServer::DebugServer(QObject *parent) :
	QObject(parent)
{
	debugServer = this;
	blocksize = 0;
	server = new QTcpServer(this);
	server->setMaxPendingConnections(1);
	connect(server, SIGNAL(newConnection()), this, SLOT(clientArrived()));
	server->listen(QHostAddress::Any, serverPortNo());
	mDebug("listening on port %d", serverPortNo());
}

void DebugServer::handleMessage(QTcpSocket *client, const QString &cmd,
								const QByteArray &)
{
	mInfo("received command: %s", qPrintable(cmd));
	QByteArray d;
	QDataStream out(&d, QIODevice::WriteOnly);
	if (cmd == "stats") {
		out << elements->size();
		out << 5; //number of properties
		for (int i = 0; i < elements->size(); i++){
			BaseLmmElement *el = elements->at(i);
			out << el->getInputBufferCount();
			out << el->getOutputBufferCount();
			out << el->getReceivedBufferCount();
			out << el->getSentBufferCount();
			out << el->getFps();
		}
		out << custom;
		sendMessage(client, "stats", d);
	} else if (cmd == "names") {
		out << (qint32)elements->size();
		for (int i = 0; i < elements->size(); i++)
			out << QString(elements->at(i)->metaObject()->className());
		sendMessage(client, "names", d);
	}
}

void DebugServer::sendMessage(QTcpSocket *client, const QString &cmd,
							  const QByteArray &data)
{
	QByteArray block;
	QDataStream out(&block, QIODevice::WriteOnly);
	out << (quint32)(cmd.length() + data.length());
	out << cmd;
	out << data;
	client->write(block);
}

void DebugServer::sendMessage(const QString &cmd, const char *msg)
{
	QByteArray block;
	QDataStream out(&block, QIODevice::WriteOnly);
	int len = strlen(msg);
	out << (quint32)(cmd.length() + len);
	out << cmd;
	out << msg;
	client->write(block);
}

void DebugServer::addCustomStat(DebugServer::CustomStat stat, int val)
{
	custom.insert(stat, val);
}

void DebugServer::clientArrived()
{
	client = server->nextPendingConnection();
	connect(client, SIGNAL(disconnected()), SLOT(clientDisconnected()));
	connect(client, SIGNAL(readyRead()), SLOT(clientDataReady()));
	qInstallMsgHandler(serverMessageOutput);
	mDebug("Client %s connected", qPrintable(client->peerAddress().toString()));
}

void DebugServer::clientDisconnected()
{
	qInstallMsgHandler(0);
	mInfo("client disconnected");
}

void DebugServer::clientDataReady()
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
	handleMessage(client, cmd, data);
	blocksize = 0;

	if (client->bytesAvailable())
		goto again;
}
