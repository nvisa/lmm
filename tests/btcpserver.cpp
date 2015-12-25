#include "btcpserver.h"

#include <lmm/debug.h>

#include <QTcpSocket>
#include <QTcpServer>
#include <QDataStream>

#include <errno.h>

BTcpServer::BTcpServer(QObject *parent) :
	QObject(parent)
{
	serv = NULL;
}

bool BTcpServer::listen(const QHostAddress &address, quint16 port)
{
	serv = new QTcpServer(this);
	connect(serv, SIGNAL(newConnection()), SLOT(newConnection()));
	return serv->listen(address, port);
}

QStringList BTcpServer::getConnectedClientIPs()
{
	QStringList addrs;
	foreach (const QTcpSocket *s, clients)
		addrs << s->peerAddress().toString();
	return addrs;
}

const QList<QTcpSocket *> BTcpServer::getConnectedClients()
{
	return clients;
}

QTcpSocket *BTcpServer::getClient(const QString &addr)
{
	foreach (QTcpSocket *s, clients)
		if (s->peerAddress().toString() == addr)
			return s;
	return NULL;
}

QTcpSocket *BTcpServer::getClient(int ind)
{
	if (ind < clients.size())
		return clients[ind];
	return NULL;
}

void BTcpServer::send(const QByteArray &data, QTcpSocket *sock)
{
	QByteArray block;
	QDataStream out(&block, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_0);
	out << (quint16)0;
	out << data;
	out.device()->seek(0);
	out << (quint16)(block.size() - sizeof(quint16));
	sock->write(block);
}

void BTcpServer::newConnection()
{
	QTcpSocket *sock = serv->nextPendingConnection();
	connect(sock, SIGNAL(disconnected()), SLOT(clientDisconnected()));
	connect(sock, SIGNAL(readyRead()), SLOT(dataReady()));
	mDebug("client %s connected", qPrintable(sock->peerAddress().toString()));
	clients << sock;
	blockSizes[sock] = 0;
}

void BTcpServer::clientDisconnected()
{
	QTcpSocket *sock = (QTcpSocket *)sender();
	clients.removeAll(sock);
	sock->deleteLater();
}

void BTcpServer::dataReady()
{
	QTcpSocket *sock = (QTcpSocket *)sender();
	mLogv("new message");

	while (sock->bytesAvailable()) {
		QDataStream in(sock);
		in.setVersion(QDataStream::Qt_4_0);

		quint16 blockSize = blockSizes[sock];
		if (blockSize == 0) {
			if (sock->bytesAvailable() < (int)sizeof(quint16))
				return;

			in >> blockSize;
			blockSizes[sock] =  blockSize;
		}

		if (sock->bytesAvailable() < blockSize)
			return;

		QByteArray data;
		in >> data;
		blockSizes[sock] = 0;
		emit newDataAvailable(sock, data);
	}
}
