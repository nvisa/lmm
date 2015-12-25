#include "btcpsocket.h"

#include <lmm/debug.h>

#include <QTcpSocket>
#include <QDataStream>

BTcpSocket::BTcpSocket(QObject *parent) :
	QObject(parent)
{
	sock = new QTcpSocket(this);
	connect(sock, SIGNAL(connected()), SIGNAL(connected()));
	connect(sock, SIGNAL(disconnected()), SLOT(clientDisconnected()));
	connect(sock, SIGNAL(readyRead()), SLOT(dataReady()));
}

QAbstractSocket::SocketState BTcpSocket::state()
{
	return sock->state();
}

void BTcpSocket::abort()
{
	sock->abort();
}

void BTcpSocket::disconnectFromHost()
{
	sock->disconnectFromHost();
}

void BTcpSocket::close()
{
	sock->close();
}

void BTcpSocket::connectToHost(const QHostAddress &address, quint16 port)
{
	sock->connectToHost(address, port);
}

QTcpSocket *BTcpSocket::socket()
{
	return sock;
}

void BTcpSocket::clientDisconnected()
{
	emit disconnected();
}

void BTcpSocket::dataReady()
{
	QTcpSocket *sock = (QTcpSocket *)sender();
	mInfo("new message");

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
		emit newDataAvailable(data);
	}
}
