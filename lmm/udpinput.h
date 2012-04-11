#ifndef UDPINPUT_H
#define UDPINPUT_H

#include <lmm/baselmmelement.h>

#include <QMap>
#include <QList>
#include <QHostAddress>

class QUdpSocket;

class UdpInput : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit UdpInput(QObject *parent = 0);
	virtual int start();
	virtual int stop();
	virtual int flush();
signals:
	void newFrameReceived(const QByteArray &);
private slots:
	void dataReady();
	void connected();
private:
	void processTheDatagram(const QByteArray &);
	void writeData(const QByteArray &data);
	void frameReceived(int frameNo, const QByteArray &hash);

	QHostAddress target;
	QUdpSocket *sock;

	QMap<int, QList<QByteArray> > frames;
	int lastIDRFrameReceived;
	int idrWaited;
};

#endif // UDPINPUT_H
