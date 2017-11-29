#include "tcpserver.h"

#include "debug.h"
#include <QTcpSocket>

TcpServer::TcpServer(quint16 port, QObject *parent)
	: QObject(parent)
{
	dport = port;
	hAddress = QHostAddress::Any;

	serv = new QTcpServer();
	serverState = serv->listen(hAddress, dport);
	if(!serverState)
		mDebug("Error listening port %s Maybe, The reason is port using", qPrintable(port));
	connect(serv, SIGNAL(newConnection()), SLOT(newConnection()));
}

void TcpServer::newConnection()
{
	QTcpSocket *sock = serv->nextPendingConnection();
	connect(sock, SIGNAL(readyRead()), SLOT(dataReady()));
	connect(sock, SIGNAL(disconnected()), SLOT(disconnected()));

	QString address = sock->peerAddress().toString().split(":").last();
	connectionList << address;
	mInfo("New Connection from this host : %s", qPrintable(address));
	socket = sock;
	sockets << sock;
}

void TcpServer::disconnected()
{
	QTcpSocket *sock = (QTcpSocket *)sender();
	sock->disconnect();
	sockets.removeOne(sock);
	mDebug("Disconnect %s", qPrintable(sock->peerAddress().toString().split(":").last()));
}

void TcpServer::dataReady()
{
	QTcpSocket *sock = (QTcpSocket *)sender();
	/**
	  ToDo: Maybe Get or Set request type */
	Q_UNUSED(sock);
}

QTcpSocket* TcpServer::getSocket()
{
	//  Return last connection
	return socket;
}

bool TcpServer::sendMetaData(QByteArray data)
{
	if(sockets.isEmpty()) {
		mInfo("Not connection device");
		return false;
	}
	if(data.isEmpty()) {
		mDebug("The data to be sent is empty");
		return false;
	}
	foreach (QTcpSocket *sock, sockets) {
		qint64 state = sock->write(data);
		if(state == 0) {
			mDebug("Data sending error");
			return false;
		}
	}
	return true;
}


