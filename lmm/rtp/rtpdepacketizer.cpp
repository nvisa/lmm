#include "rtpdepacketizer.h"
#include "debug.h"
#include "streamtime.h"

#include <QUdpSocket>
#include <QElapsedTimer>
#include <QNetworkInterface>

#include <errno.h>

#define RTP_VERSION 2

RtpDePacketizer::RtpDePacketizer(QObject *parent) :
	BaseLmmElement(parent)
{
	maxPayloadSize = 1460;
	annexPrefix.append(char(0));
	annexPrefix.append(char(0));
	annexPrefix.append(char(0));
	annexPrefix.append(char(1));
}

int RtpDePacketizer::start()
{
	/* Let's find our IP address */
	foreach (const QNetworkInterface iface, QNetworkInterface::allInterfaces()) {
		if (iface.name() != "eth0")
			continue;
		if (iface.addressEntries().size()) {
			myIpAddr = iface.addressEntries().at(0).ip();
			break;
		}
	}
	sock = new QUdpSocket;
	sock->bind(srcDataPort);
	connect(sock, SIGNAL(readyRead()), SLOT(readPendingRtpDatagrams()));
	seqLast = 0;
	currentNal = annexPrefix;

	return BaseLmmElement::start();
}

int RtpDePacketizer::stop()
{
	streamLock.lock();
	sock->deleteLater();
	streamLock.unlock();
	return BaseLmmElement::stop();
}

void RtpDePacketizer::readPendingRtpDatagrams()
{
	streamLock.lock();
	while (sock && sock->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(sock->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		sock->readDatagram(datagram.data(), datagram.size(),
								&sender, &senderPort);
		/* we may receive our own messages when using multicast */
		if (sender == myIpAddr)
			continue;
		/* TODO: check incoming message type */
		processRtpData(datagram);
		//emit newReceiverReport(srcControlPort);
	}
	streamLock.unlock();
}

int RtpDePacketizer::processBuffer(const RawBuffer &buf)
{
	Q_UNUSED(buf);
	return -EPERM;
}

int RtpDePacketizer::processRtpData(const QByteArray &ba)
{
	const uchar *buf = (const uchar *)ba.constData();
	if (buf[0] != RTP_VERSION << 6) {
		mDebug("un-expected RTP version %d", buf[0] >> 6);
		return -EINVAL;
	}
	int ptype = buf[1] & 0x7f;
	if (ptype != 96) {
		mDebug("un-supported payload type %d", ptype);
		return -EINVAL;
	}
	int last = buf[1] >> 7;
	uint seq = buf[2] << 8 | buf[3];
	uint ts = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
	uint ssrc = (buf[8] << 24) | (buf[9] << 16) | (buf[10] << 8) | buf[11];
	if (ssrc != 0x78457845) {
		mDebug("un-expected SSRC 0x%x", ssrc);
		return -EINVAL;
	}
	if (seqLast && seq != seqLast + 1)
		mDebug("detected a jump in RTP frames");
	seqLast = seq;

	int fui = (buf[12] & 0x1f);
	if (fui == 28) {
		uchar nri = buf[12] & 0x60;
		uchar type = buf[13] & 0x1f;
		int start = buf[13] >> 7;
		int end = (buf[13] >> 6) & 0x1;
		if (start && currentNal.size() != annexPrefix.size()) {
			mDebug("fixing buffer size mismatch at FUI start: %d != %d", currentNal.size(), annexPrefix.size());
			currentNal = annexPrefix;
		}
		if (start)
			currentNal.append(type | nri);
		currentNal.append(ba.mid(14));
		if (end && !last)
			ffDebug() << "end and last mismatch";
		if (!end && last)
			ffDebug() << "last and end mismatch";
	}
	else
		currentNal.append(ba.mid(12));
	if (last) {
		mInfo("received valid RTP frame with ts %u and size %d", ts, currentNal.size());
		RawBuffer buf("application/x-rtp", currentNal.constData(), currentNal.size());
		buf.pars()->videoWidth = 1280;
		buf.pars()->videoHeight = 720;
		buf.pars()->encodeTime = streamTime->getCurrentTime();
		newOutputBuffer(0, buf);
		currentNal = annexPrefix;
	}

	return 0;
}
