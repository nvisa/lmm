#include "udpoutput.h"

#include <emdesk/debug.h>

#include <QUdpSocket>
#include <QCryptographicHash>

UdpOutput::UdpOutput(QObject *parent) :
	BaseLmmOutput(parent)
{
}

int UdpOutput::start()
{
	clientStatus = CS_UNKNOWN;
	target = QHostAddress("192.168.1.1");
	sock = new QUdpSocket(this);
	sock->bind(47156);
	connect(sock, SIGNAL(readyRead()), SLOT(dataReady()));
	return BaseLmmOutput::start();
}

int UdpOutput::stop()
{
	sock->deleteLater();
	clientStatus = CS_UNKNOWN;
	return BaseLmmOutput::stop();
}

QByteArray UdpOutput::createControlCommand(UdpOutput::CommandType cmd, QVariant)
{
	QByteArray ba;
	ba.append(PT_CP);
	ba.append(cmd);
	ba.append((char)0);
	ba.append((char)0);
	ba.append((char)0);
	ba.append((char)0);
	ba.append((char)0);
	ba.append((char)0);
	return ba;
}

void UdpOutput::dataReady()
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

int UdpOutput::outputBuffer(RawBuffer buf)
{
	if (clientStatus != CS_STARTED)
		return 0;
	/* ASSUMPTION: We always receive one and only one encoded stream */
	QByteArray ba;
	char *frame = (char *)buf.constData();
	int size = buf.size();
	int cnt = size / DATAGRAM_SIZE + 1;
	// send ses
	int sendCnt = 0;
	int no = buf.streamBufferNo() & 0xffff;
	ba.append(PT_SES);
	ba.append(no >> 8);
	ba.append(no & 0xff);
	ba.append(cnt >> 8);
	ba.append(cnt & 0xff);
	ba.append((size >> 16) & 0xff);
	ba.append((size >> 8) & 0xff);
	ba.append(size & 0xff);
	ba.append(frame + sendCnt, size > DATAGRAM_SIZE ? DATAGRAM_SIZE : size);
	writeData(ba);
	sendCnt += ba.size() - HEADER_SIZE;
	ba.clear();
	for (int i = 1; i < cnt - 1; i++) {
		ba.append(PT_ESB);
		ba.append(no >> 8);
		ba.append(no & 0xff);
		ba.append(i >> 8);
		ba.append(i & 0xff);
		ba.append((char)0);
		ba.append((char)0);
		ba.append((char)0);
		ba.append(frame + sendCnt, DATAGRAM_SIZE);
		sendCnt += ba.size() - HEADER_SIZE;
		writeData(ba);
		ba.clear();
	}
	if (cnt > 1) {
		// send ees
		ba.append(PT_EES);
		ba.append(no >> 8);
		ba.append(no & 0xff);
		ba.append((char)buf.getBufferParameter("frameType").toInt());
		ba.append((char)0);
		ba.append((char)0);
		ba.append((char)0);
		ba.append((char)0);
		ba.append(frame + sendCnt, size - sendCnt);
		writeData(ba);
		ba.clear();
		if (buf.getBufferParameter("frameType").toInt() != 1) {
			mInfo("frame type is %d",
				   buf.getBufferParameter("frameType").toInt());
		}
	}

	QCryptographicHash hash(QCryptographicHash::Md5);
	hash.addData((const char *)buf.constData());
	QByteArray hashBa = hash.result();
	ba.append(PT_HES);
	ba.append(no >> 8);
	ba.append(no & 0xff);
	ba.append((char)0);
	ba.append((char)0);
	ba.append((char)0);
	ba.append((char)0);
	ba.append((char)0);
	ba.append(hashBa);
	writeData(ba);

	return 0;
}

/*
 * Remember 2 things:
 *
 *	- UDP packets can(should) not be larger than eth packets, i.e 1520
 *	- Datagrams can reach to the destination out of order
 *
 *
 * We want to transmit 2 type of data:
 *
 *	- Encoded video streams
 *	- Control packets
 *
 * We split encoded video data into chunks, then we put a header
 * into the every datagram. Receiver and than will use information
 * in headers to merge datagrams into a new encoded frame.
 *
 * Datagram header is 8 bytes:
 *	- 1 byte packet type, can be one of the following:
		a. start of encoded stream(ses)
		b. end of encoded stream(ees)
		c. encoded stream buffer(esb)
		e. hash of encoded stream(hes)
		d. control packet(cp)
	Next bytes are dependent on packet type.
	For ses:
		- 2 bytes for frame number: which encoded frame this packet belongs to
		- 2 bytes for total number of datagrams for this stream
		- 3 bytes for byte count: size of this frame in bytes
	For ees:
		- 2 bytes for frame number: which encoded frame this packet belongs to
		- 1 byte for buffer type: I, P or B frame
	For esb:
		- 2 bytes for frame number: which encoded frame this packet belongs to
		- 2 bytes for datagram number: position of this datagram for this frame
*/

void UdpOutput::processTheDatagram(const QByteArray &data)
{
	if (data[0] == PT_CP) {
		if (data[1] == CT_START)
			clientStatus = CS_STARTED;
		else if (data[1] == CT_FLUSH) {
			qDebug("flush command received");
			emit needFlushing();
		}
	}
}

void UdpOutput::writeData(const QByteArray &data)
{
	sock->writeDatagram(data, target, 47156);
}
