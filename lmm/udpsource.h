#ifndef UDPSOURCE_H
#define UDPSOURCE_H

#include <lmm/baselmmelement.h>

#include <QString>

class QUdpSocket;

class UdpSource : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit UdpSource(QObject *parent = 0);
	virtual int start();
	virtual int stop();
	virtual int flush();
	int setHostAddress(QString addr);
	int setPort(int port);
signals:
	
protected slots:
	void dataReady();
protected:
	void processTheDatagram(const QByteArray &ba);
	int processBuffer(RawBuffer);

	QUdpSocket *sock;
	int bindPort;
	QString hostAddress;
};

#endif // UDPSOURCE_H
