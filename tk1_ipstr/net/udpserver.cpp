#include "debug.h"
#include "udpserver.h"

UdpServer::UdpServer(quint16 udpPort, QObject *parent)
	: QObject(parent)
{
	port = udpPort;
	hAddress = QHostAddress::Any;
	uSenderPort = 0;
	socket = new QUdpSocket(this);
	sockState = socket->bind(hAddress, port, QUdpSocket::DontShareAddress);
	connect(socket, SIGNAL(readyRead()), SLOT(readyRead()));
	connect(socket, SIGNAL(bytesWritten(qint64)), SLOT(bytesWritten(qint64)));

	if(!sockState)
		mDebug("Error bind socket... Maybe  reason is wrong ip  %s or use port %s",qPrintable(hAddress.toString()),qPrintable(port));
}

void UdpServer::bytesWritten(qint64 bytes)
{
	Q_UNUSED(bytes);
}

void UdpServer::readyRead()
{
	while (socket->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(socket->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;
		socket->readDatagram(datagram.data(), datagram.size(),
								&sender, &senderPort);
		uSender = sender;
		uSenderPort = senderPort;
		if((!uSender.isNull()) & (uSenderPort != 0)) {
			uSenderInfo.insert(uSender, uSenderPort);
		}
	}
}

bool UdpServer::sendMetaData(QByteArray ba)
{
	quint64 state = 0;
	if(uSenderInfo.isEmpty()) {
		mInfo("There is no connection. Connected device list is empty");
		return false;
	}

	foreach (QHostAddress address, uSenderInfo.keys()) {
		if(address.isNull())
			continue;
		if(uSenderInfo.value(address) == 0)
			continue;
		if(ba.size())
			state = socket->writeDatagram(ba, address, uSenderInfo.value(address));
	}

	if(state == 0) {
		mDebug("Data sending error");
		return false;
	}
	return true;
}
