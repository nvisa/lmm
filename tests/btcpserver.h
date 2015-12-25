#ifndef BTCPSERVER_H
#define BTCPSERVER_H

#include <QObject>
#include <QHostAddress>

class QTcpServer;
class QTcpSocket;

class BTcpServer : public QObject
{
	Q_OBJECT
public:
	explicit BTcpServer(QObject *parent = 0);

	bool listen(const QHostAddress &address = QHostAddress::Any, quint16 port = 0);
	QStringList getConnectedClientIPs();
	const QList<QTcpSocket *> getConnectedClients();
	QTcpSocket * getClient(const QString &addr);
	QTcpSocket * getClient(int ind);

	static void send(const QByteArray &data, QTcpSocket *sock);

signals:
	void newDataAvailable(QTcpSocket *sock, const QByteArray &data);

protected slots:
	void newConnection();
	void clientDisconnected();
	void dataReady();

protected:
	QTcpServer *serv;
	QList<QTcpSocket *> clients;
	QHash<QTcpSocket *, quint16> blockSizes;
};

#endif // BTCPSERVER_H
