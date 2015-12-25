#ifndef BTCPSOCKET_H
#define BTCPSOCKET_H

#include <QHash>
#include <QObject>
#include <QAbstractSocket>

class QTcpSocket;

class BTcpSocket : public QObject
{
	Q_OBJECT
public:
	explicit BTcpSocket(QObject *parent = 0);
	QAbstractSocket::SocketState state();
	void abort();
	void disconnectFromHost();
	void close();
	void connectToHost(const QHostAddress &address, quint16 port);
	QTcpSocket * socket();

signals:
	void connected();
	void disconnected();
	void newDataAvailable(const QByteArray &data);

protected slots:
	void clientDisconnected();
	void dataReady();
protected:
	QTcpSocket *sock;
	QHash<QTcpSocket *, quint16> blockSizes;
};

#endif // BTCPSOCKET_H
