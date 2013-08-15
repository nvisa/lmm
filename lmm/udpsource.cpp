#include "udpsource.h"

#include <QUdpSocket>

#include <lmm/debug.h>

UdpSource::UdpSource(QObject *parent) :
	BaseLmmElement(parent)
{
	bindPort = 4958;
	sock = NULL;
}

int UdpSource::start()
{
	sock = new QUdpSocket(this);
	sock->bind(bindPort);
	connect(sock, SIGNAL(readyRead()), SLOT(dataReady()));
	return BaseLmmElement::start();
}

int UdpSource::stop()
{
	if (sock) {
		sock->deleteLater();
		sock = NULL;
	}
	return BaseLmmElement::stop();
}

int UdpSource::flush()
{
	return BaseLmmElement::flush();
}

int UdpSource::setHostAddress(QString addr)
{
	hostAddress = addr;
	return 0;
}

int UdpSource::setPort(int port)
{
	bindPort = port;
	return 0;
}

void UdpSource::dataReady()
{
	while (sock->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(sock->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		sock->readDatagram(datagram.data(), datagram.size(),
								&sender, &senderPort);

		processTheDatagram(datagram);
	}
}

void UdpSource::processTheDatagram(const QByteArray &ba)
{
	RawBuffer buf("application/x-rtp", ba.constData(), ba.size());
	mInfo("new packet received with size %d", buf.size());
	outputLock.lock();
	outputBuffers << buf;
	releaseOutputSem(0);
	outputLock.unlock();
}
