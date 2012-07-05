#include "debugserver.h"
#include "baselmmelement.h"
#include "tools/unittimestat.h"

#include <emdesk/debug.h>

#include <QTcpServer>
#include <QTcpSocket>
#include <QDateTime>
#include <QProcess>

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
								const QByteArray &data)
{
	mInfo("received command: %s", qPrintable(cmd));
	QByteArray d;
	QDataStream out(&d, QIODevice::WriteOnly);
	if (cmd == "stats") {
		out << elements->size();
		out << 8; //number of properties
		for (int i = 0; i < elements->size(); i++){
			BaseLmmElement *el = elements->at(i);
			out << el->getInputBufferCount();
			out << el->getOutputBufferCount();
			out << el->getReceivedBufferCount();
			out << el->getSentBufferCount();
			out << el->getFps();
			out << el->getOutputTimeStat()->min;
			out << el->getOutputTimeStat()->max;
			out << el->getOutputTimeStat()->avg;
		}
		out << custom;
		sendMessage(client, "stats", d);
	} else if (cmd == "names") {
		out << (qint32)elements->size();
		for (int i = 0; i < elements->size(); i++)
			out << QString(elements->at(i)->metaObject()->className());
		sendMessage(client, "names", d);
	} else if (cmd == "syncTime") {
		QDateTime dt = QDateTime::fromString(QString(data));
		QProcess::execute(QString("date %1").arg(dt.toString("MMddhhmmyyyy.ss")));
	} else
		emit newApplicationMessage(client, cmd, data);
}

int DebugServer::sendMessage(QTcpSocket *client, const QString &cmd,
							  const QByteArray &data)
{
	QByteArray block;
	QDataStream out(&block, QIODevice::WriteOnly);
	quint32 size = 2 * sizeof(quint32) + cmd.length() * 2 + data.length() + 4;
	out << size;
	out << cmd;
	out << data;
	return client->write(block);
}

QString DebugServer::getStatisticsString(DebugServer::CustomStat stat)
{
	if (stat == STAT_LOOP_TIME)
		return "Loop Time";
	if (stat == STAT_OVERLAY_TIME)
		return "Overlay Time";
	if (stat == STAT_CAPTURE_TIME)
		return "Capture Time";
	if (stat == STAT_ENCODE_TIME)
		return "Encode Time";
	if (stat == STAT_RTSP_OUT_TIME)
		return "RTSP Send Time";
	if (stat == STAT_DISP_OUT_TIME)
		return "Display Output Time";
	return "N/A";
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

int DebugServer::addCustomStat(DebugServer::CustomStat stat, int val)
{
	if (val > 2)
		mInfo("%s: %d msecs", qPrintable(getStatisticsString(stat)), val);
	custom.insert(stat, val);
	return val > 2 ? 1 : 0;
}

void DebugServer::clientArrived()
{
	client = server->nextPendingConnection();
	connect(client, SIGNAL(disconnected()), SLOT(clientDisconnected()));
	connect(client, SIGNAL(readyRead()), SLOT(clientDataReady()));
	//qInstallMsgHandler(serverMessageOutput);
	mDebug("Client %s connected", qPrintable(client->peerAddress().toString()));
}

void DebugServer::clientDisconnected()
{
	//qInstallMsgHandler(0);
	client->deleteLater();
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
