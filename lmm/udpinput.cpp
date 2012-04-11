#include "udpinput.h"
#include "udpoutput.h"

#include <QUdpSocket>
#include <QCryptographicHash>

UdpInput::UdpInput(QObject *parent) :
	BaseLmmElement(parent)
{
}

int UdpInput::start()
{
	idrWaited = 0;
	target = QHostAddress("192.168.1.199");
	lastIDRFrameReceived = 0;
	sock = new QUdpSocket(this);
	sock->bind(47156);
	sock->writeDatagram(UdpOutput::createControlCommand(UdpOutput::CT_FLUSH)
						, target, 47156);
	connect(sock, SIGNAL(readyRead()), SLOT(dataReady()));
	connect(sock, SIGNAL(connected()), SLOT(connected()));
	return 0;//TODO: Find the reason: BaseLmmElement::start();
}

int UdpInput::stop()
{
	sock->deleteLater();
	return BaseLmmElement::stop();
}

int UdpInput::flush()
{
	idrWaited = 0;
	lastIDRFrameReceived = 0;
	sock->writeDatagram(UdpOutput::createControlCommand(UdpOutput::CT_FLUSH)
						, target, 47156);
	return 0;//BaseLmmElement::flush();
}

void UdpInput::dataReady()
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

void UdpInput::connected()
{
	sock->write(UdpOutput::createControlCommand(UdpOutput::CT_FLUSH));
	qDebug() << "connected";
}

void UdpInput::processTheDatagram(const QByteArray &ba)
{
	const uchar *data = (const uchar *)ba.constData();
	if (data[0] == UdpOutput::PT_SES) {
		int frameNo = (data[1] << 8) + data[2];
		int dgrmCnt = (data[3] << 8) + data[4];
		frames.insert(frameNo, QList<QByteArray>());
		for (int i = 0; i < dgrmCnt; i++)
			frames[frameNo] << QByteArray();
		frames[frameNo][0] = ba;
	} else if (data[0] == UdpOutput::PT_ESB) {
		int frameNo = (data[1] << 8) + data[2];
		if (frames.contains(frameNo)) {
			int dgrmNo = (data[3] << 8) + data[4];
			frames[frameNo][dgrmNo] = ba;
			if (ba.size() != DATAGRAM_SIZE + HEADER_SIZE)
				qDebug() << "wrong esb size" << ba.size();
		}
	} else if (data[0] == UdpOutput::PT_EES) {
		int frameNo = (data[1] << 8) + data[2];
		if (frames.contains(frameNo)) {
			int size = frames[frameNo].size();
			frames[frameNo][size - 1] = ba;
			if (data[3] == 3) {
				qDebug() << "new idr frame";
				lastIDRFrameReceived = frameNo;
			}
		}
	} else if (data[0] == UdpOutput::PT_HES) {
		int frameNo = (data[1] << 8) + data[2];
		if (frames.contains(frameNo))
			frameReceived(frameNo, ba.mid(HEADER_SIZE));
	}
}

void UdpInput::frameReceived(int frameNo, const QByteArray &hash)
{
	QByteArray frame;
	const QList<QByteArray> list = frames[frameNo];
	const uchar *data = (const uchar *)list[0].constData();
	int frameNoSrc = (data[1] << 8) + data[2];
	int dgrmCnt = (data[3] << 8) + data[4];
	int byteCnt = (data[5] << 16) + (data[6] << 8)+ data[7];
	if (frameNoSrc != frameNo)
		qDebug() << "frame mismatch" << frameNoSrc << frameNo;
	if (list.size() != dgrmCnt)
		qDebug() << "count mismatch";
	for (int i = 0; i < list.size(); i++)
		frame.append(list[i].mid(HEADER_SIZE));
	if (byteCnt != frame.size())
		qDebug() << "byte count mismatch:" << frame.size() << byteCnt << dgrmCnt;
	QCryptographicHash hashChk(QCryptographicHash::Md5);
	hashChk.addData((const char *)frame);
	QByteArray hashBa = hashChk.result();
	if (hashBa != hash)
		qDebug() << "hash mismatch";
	if (lastIDRFrameReceived && frameNo >= lastIDRFrameReceived) {
		idrWaited = 0;
		emit newFrameReceived(frame);
	} else {
		idrWaited++;
		if (idrWaited >= 5)
			flush();
		qDebug() << "waiting idr frame";
	}

	frames.remove(frameNo);
}
