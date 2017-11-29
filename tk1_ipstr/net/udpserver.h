#ifndef UDPSERVER_H
#define UDPSERVER_H

#include <QUdpSocket>

class UdpServer : public QObject
{
	Q_OBJECT
public:
	explicit UdpServer(quint16 udpPort, QObject *parent = 0);

	bool sendMetaData(QByteArray ba);
signals:

public slots:
protected slots:
	void readyRead();
	void bytesWritten(qint64);
private:
	QUdpSocket *socket;
	QHostAddress hAddress;
	int port;
	bool sockState;
	QHostAddress uSender;
	quint16 uSenderPort;
	QHash<QHostAddress, quint16> uSenderInfo;
};

#endif // UDPSERVER_H
