#include "rtpreceiver.h"
#include "debug.h"
#include "rawbuffer.h"

#include <QFile>
#include <QDateTime>
#include <QUdpSocket>

#include <errno.h>
#include <sys/time.h>

static inline ushort getUInt16BE(const uchar *buf)
{
	return (buf[0] << 8) + (buf[1] << 0);
}

static inline uint getUInt32BE(const uchar *buf)
{
	return (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + (buf[3] << 0);
}

#define RTP_VERSION 2
#define NTP_OFFSET 2208988800ULL  //ntp is from 1900 instead of 1970

RtpReceiver::RtpReceiver(QObject *parent) :
	BaseLmmElement(parent)
{
	maxPayloadSize = 1460;
	annexPrefix.append(char(0));
	annexPrefix.append(char(0));
	annexPrefix.append(char(0));
	annexPrefix.append(char(1));

	currentNal = annexPrefix;

	passThru = true;
	srcDataPort = 0;
	srcControlPort = 0;
	sock = NULL;
}

RtpReceiver::RtpStats RtpReceiver::getStatistics()
{
	return stats;
}

int RtpReceiver::start()
{
#if QT_VERSION > 0x050000
	sock = new QUdpSocket(this);
	sock2 = new QUdpSocket(this);
	if (!sock->bind(QHostAddress::AnyIPv4, srcDataPort)) {
		sock->deleteLater();
		sock2->deleteLater();
		sock = NULL;
		sock2 = NULL;
		return -EPERM;
	}
	if (!sock2->bind(QHostAddress::AnyIPv4, srcControlPort)) {
		sock->deleteLater();
		sock2->deleteLater();
		sock = NULL;
		sock2 = NULL;
		return -EPERM;
	}
	connect(sock, SIGNAL(readyRead()), SLOT(readPendingRtpDatagrams()));
	connect(sock2, SIGNAL(readyRead()), SLOT(readPendingRtcpDatagrams()));

	if (sourceAddr.isInSubnet(QHostAddress::parseSubnet("224.0.0.0/3"))) {
		if (!sock->joinMulticastGroup(sourceAddr))
			ffDebug() << trUtf8("Error joining multicast port, maybe it in use by another program?");
		if (!sock2->joinMulticastGroup(sourceAddr))
			ffDebug() << trUtf8("Error joining multicast port, maybe it in use by another program?");
	}

	stats.packetCount = 0;
	stats.packetMissing = 0;
	seqLast = 0;
	rtcpTime.start();
#endif
	return 0;
}

int RtpReceiver::stop()
{
	if (sock) {
		sock->close();
		sock2->close();
		sock->deleteLater();
		sock2->deleteLater();
		sock = sock2 = NULL;
	}
	return 0;
}

void RtpReceiver::readPendingRtpDatagrams()
{
	while (sock && sock->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(sock->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		sock->readDatagram(datagram.data(), datagram.size(),
								&sender, &senderPort);
		processRtpData(datagram, QHostAddress(sender.toIPv4Address()), senderPort);
	}
}

void RtpReceiver::readPendingRtcpDatagrams()
{
	while (sock2 && sock2->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(sock2->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		sock2->readDatagram(datagram.data(), datagram.size(),
								&sender, &senderPort);

		const uchar *buf = (const uchar *)datagram.constData();
		if (buf[0] >> 6 != RTP_VERSION) {
			stats.rtcpVersionError++;
			continue;
		}

		if (buf[1] == 200) {
			int length = getUInt16BE(&buf[2]);
			uint ssrc = getUInt32BE(&buf[4]);
			uint ntps = getUInt32BE(&buf[8]);
			uint ntpf = getUInt32BE(&buf[12]);
			uint rtpts = getUInt32BE(&buf[16]);
			uint pcnt = getUInt32BE(&buf[20]);
			uint ocnt = getUInt32BE(&buf[24]);

			//struct timeval tv;
			//gettimeofday(&tv, NULL);
			//qint64 ntpu = (qint64)tv.tv_sec * 1000000 + tv.tv_usec + NTP_OFFSET * 1000000;
			uint tv_sec = ntps - NTP_OFFSET;
			uint tv_usec = (double)ntpf * 15625 / 0x04000000;

#if QT_VERSION > 0x050000
			qint64 timet = (qint64)tv_sec * 1000 + tv_usec / 1000;
			stats.rtcpEpoch = timet;
			stats.rtcpTs = rtpts;
			stats.rtcpMyEpoch = QDateTime::currentDateTime().toMSecsSinceEpoch();
			//qint64 epoch = QDateTime::currentDateTime().toMSecsSinceEpoch();
			//qDebug() << (epoch - timet) << timet << epoch << tv_sec << tv_usec << tv.tv_sec << tv.tv_usec;
#endif
		}


		/* ntp part */
		/*struct timeval tv;
		gettimeofday(&tv, NULL);
		qint64 ntpu = (qint64)tv.tv_sec * 1000000 + tv.tv_usec + NTP_OFFSET * 1000000;
		uint ntps = tv.tv_sec + NTP_OFFSET;
		uint ntpf = (uint)((tv.tv_usec / 15625.0 ) * 0x04000000 + 0.5);*/
	}
}

int RtpReceiver::processRtpData(const QByteArray &ba, const QHostAddress &sender, quint16 senderPort)
{
	const uchar *buf = (const uchar *)ba.constData();
	if (buf[0] != RTP_VERSION << 6) {
		mDebug("un-expected RTP version %d", buf[0] >> 6);
		stats.headerError++;
		return -EINVAL;
	}
	int ptype = buf[1] & 0x7f;
	if (ptype != 96 && ptype != 98) {
		stats.payloadErr++;
		mDebug("un-supported payload type %d", ptype);
		return -EINVAL;
	}
	int last = buf[1] >> 7;
	uint seq = buf[2] << 8 | buf[3];
	uint ts = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
	uint ssrc = (buf[8] << 24) | (buf[9] << 16) | (buf[10] << 8) | buf[11];
	if (rtcpTime.elapsed() > 5000)
		sendRR(ssrc, sender);

	/* handle out of order delivery */
	rtpData[seq] = ba;

	QList<uint> keys = rtpData.keys();
	uint key = keys.first();
	bool next = false;
	if (stats.packetCount == 0 || key == seqLast + 1 || rtpData.size() > 50)
		next = true;

	if (!next)
		return 0;

	QByteArray ordered = rtpData.take(key);
	handleRtpData(ordered);

	/* empty order queue if possible */
	while (keys.size() && keys.first() == key + 1) {
		key = keys.takeFirst();
		ordered = rtpData.take(key);
		handleRtpData(ordered);
	}

	return 0;
}

int RtpReceiver::handleRtpData(const QByteArray &ba)
{
	const uchar *buf = (const uchar *)ba.constData();
	int ptype = buf[1] & 0x7f;
	int last = buf[1] >> 7;
	uint seq = buf[2] << 8 | buf[3];
	uint ts = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];

	if (seqLast && seq != seqLast + 1) {
		int missing = seq - seqLast - 1;
		ffDebug() << seq << seqLast + 1 << missing;
		if (missing < 0) {
		} else
			stats.packetMissing +=  missing;
		mDebug("detected a jump in RTP frames");
	}
	//qDebug() << seq << seqLast;
	seqLast = seq;
	stats.packetCount++;
	if (ptype == 96)
		processh264Payload(ba, ts, last);
	else if (ptype == 98)
		processMetaPayload(ba, ts, last);

	return 0;
}

int RtpReceiver::processh264Payload(const QByteArray &ba, uint ts, int last)
{
	const uchar *buf = (const uchar *)ba.constData();
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
		RawBuffer buf("application/x-rtp", currentNal.constData(), currentNal.size());
		buf.pars()->pts = ts;
		getInputQueue(0)->addBuffer(buf, this);
		//newOutputBuffer(0, buf);
		currentNal = annexPrefix;
	}

	return 0;
}

int RtpReceiver::processMetaPayload(const QByteArray &ba, uint ts, int last)
{
	currentData.append(ba.mid(12));
	if (last) {
#if 0
		if (!stream)
			stream = MemoryStream::joinStream(streamDesc, MemoryStream::READ_WRITE, this);
		if (stream) {
			int err = stream->produceBuffer(currentData.constData(), currentData.size());
			if (err)
				ffDebug() << "producer error" << err;
		}
#endif
		/* done */
		currentData.clear();
	}

	return 0;
}

int RtpReceiver::processBuffer(const RawBuffer &buf)
{
	/* we shouldn't get here */
	Q_UNUSED(buf);
	return 0;
}

void RtpReceiver::sendRR(uint ssrc, const QHostAddress &sender)
{
	int length = 6; //in words, 7 - 1
	uchar buf[(length + 1) * 4];
	buf[0] = RTP_VERSION << 6;
	buf[1] = 201; //RTCP_RR
	buf[2] = length >> 8;
	buf[3] = length >> 0;
	buf[4] = (ssrc >> 24) & 0xff;
	buf[5] = (ssrc >> 16) & 0xff;
	buf[6] = (ssrc >> 8) & 0xff;
	buf[7] = (ssrc >> 0) & 0xff;
	sock2->writeDatagram((const char *)buf, (length + 1) * 4, sender, dstControlPort);
	rtcpTime.restart();
}
