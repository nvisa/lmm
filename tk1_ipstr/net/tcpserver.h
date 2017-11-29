#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QTcpServer>

class TcpServer : public QObject
{
	Q_OBJECT
public:
	explicit TcpServer(quint16 port, QObject *parent = 0);

	QTcpSocket *getSocket();
	bool sendMetaData(QByteArray data);
signals:

public slots:
protected slots:
	void dataReady();
	void disconnected();
	void newConnection();
private:
	QTcpServer *serv;
	QTcpSocket *socket;
	quint16 dport;
	bool serverState;
	QHostAddress hAddress;
	QList<QTcpSocket *> sockets;
	QList<QString> connectionList;
};


#endif // TCPSERVER_H
