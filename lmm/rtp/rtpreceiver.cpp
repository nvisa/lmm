#include "rtpreceiver.h"
#include "debug.h"
#include "rawbuffer.h"
#include "h264parser.h"

#include <QFile>
#include <QDateTime>
#include <QUdpSocket>
#include <QDataStream>

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
	expectedFrameRate = 0;

	seistats.enabled = false;
	bsum.summarizeBitrateStats = false;
}

RtpReceiver::RtpStats RtpReceiver::getStatistics()
{
	return stats;
}

RtpReceiver::SeiStats RtpReceiver::getLastSeiData()
{
	return seistats;
}

void RtpReceiver::enableBitrateSummarization(bool v, int interval)
{
	bsum.summarizeBitrateStats = v;
	bsum.interval = interval;
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
	rtpPacketOffset = 0;
	containsMissing = false;
	validFrameCount = 0;
	framingError = false;
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

		if (bsum.summarizeBitrateStats) {
			if (!bsum.total)
				bsum.slot.start();
			if (bsum.slot.elapsed() > bsum.interval) {
#if QT_VERSION > 0x050000
				emit newSummarizationDataReady(bsum.total * 1000 * 8 / bsum.slot.elapsed(), QDateTime::currentMSecsSinceEpoch());
#endif
				bsum.total = 0;
				bsum.slot.restart();
			}
			bsum.total += datagram.size();
		}

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

	bool rollover = (seqLast == 65535 && seq == 0);
	if (stats.packetCount && seq != seqLast + 1 && !rollover) {
		int missing = seq - seqLast - 1;
		if (missing < 0) {
		} else
			stats.packetMissing +=  missing;
		mDebug("%s: detected a jump in RTP frames, expected=%d got=%d missing=%d",
			   qPrintable(sourceAddr.toString()), seqLast + 1, seq, qAbs(missing));
		if (seqLast > seq)
			rollover = true;
		containsMissing = true;
	}
	if (rollover)
		rtpPacketOffset += 65536;
	seqLast = seq;
	stats.packetCount++;
	if (ptype == 96)
		processh264Payload(ba, ts, last);
	else if (ptype == 98)
		processMetaPayload(ba, ts, last);

	return 0;
}

void RtpReceiver::h264FramingError()
{
	/*
	 * Case 1: Frame resides in single RTP packet
	 *
	 *		- A. We should understand the problem from seq jump i.e.
	 *		  containsMissing will be true. This should result in
	 *		  1 frame loss.
	 *
	 * Case 2: Frame occupies multiple RTP packets, FU-A'ing
	 *
	 *		- A. if we loose a regular packet, no start and no last fu-a,
	 *		  then containsMissing will be true. This means we exactly
	 *		  lost 1 frame.
	 *
	 *		- B. if we loose FU-A start, currentNal size will be 4 instead
	 *		  of 5 so we understand we lost start with the next packet.
	 *		  This should result in 1 frame loss.
	 *
	 *		- C. if we loose FU-A end, currentNal size will not be equal
	 *		  to annexPrefix size at the next FU-A start. This should
	 *		  result in 1 frame loss.
	 *
	 *		- D. if we loose both FU-A start and end, then multiple frames will
	 *		  be merged into 1 but containsMissing will be true. This should
	 *		  result in multiple frame loss.
	 */
	framingError = true;
}

int RtpReceiver::getRtpClock()
{
	return 90000; //default
}

static inline qint32 deserH264IntData(QDataStream &in)
{
	qint32 val;
	in >> val;
	qint32 eprev; in >> eprev;
	return val;
}

static bool isSupportedVersion(qint32 ver)
{
	if (ver < 0x11220105)
		return false;
	if (ver > 0x11220106)
		return false;
	return true;
}

void RtpReceiver::parseSeiUserData(const RawBuffer &buf)
{
	if (!seistats.enabled)
		return;
	const uchar *data = (const uchar *)buf.constData();
	int nal = SimpleH264Parser::getNalType(data);
	if (nal != 6)
		return;
	int off = 5;
	int sei = data[off++];
	if (sei != 5)
		return;
	int seiSize = 0;
	while (data[off] == 0xff)
		seiSize += data[off++];
	seiSize += data[off++];
	/* skip uuid */
	off += 16;

	if (seiSize - off <= 0) {
		mDebug("bad SEI data: size=%d off=%d", seiSize, off);
		return;
	}
	QByteArray ba = QByteArray::fromRawData((const char *)&data[off], buf.size() - off);
	QDataStream in(ba);
	in.setByteOrder(QDataStream::LittleEndian);
	in.setFloatingPointPrecision(QDataStream::SinglePrecision);
	qint32 key; in >> key;
	qint32 ver; in >> ver;
	if (key != 0x78984578 && !isSupportedVersion(ver)) {
		mDebug("key=0x%x or version=0x%x mismatch", key, ver);
		return;
	}
	qint32 cpuload = deserH264IntData(in);
	qint32 freeMem = deserH264IntData(in);
	qint32 uptime = deserH264IntData(in);
	qint32 pid = deserH264IntData(in);
	qint32 encoderCount = deserH264IntData(in);
	qint32 sessionCount = deserH264IntData(in) - 5;
	QList<qint32> counts;
	qint32 pipeCount = deserH264IntData(in) - 5;

	if (pipeCount) {
		for (int i = 0; i < pipeCount; i++) {
			qint32 index = deserH264IntData(in) - 5;
			qint32 bytes = deserH264IntData(in) - 5;
			if (index != i) {
				mDebug("pipe index mismatch");
				return;
			}
			counts << bytes;
		}
	}
	QByteArray meta;
	if (ver >= 0x11220106)
		in >> meta;

	seistats.cpuload = cpuload;
	seistats.bufferUsage = 0;
	for (int i = 0; i < counts.size(); i++)
		seistats.bufferUsage += counts[i];
	seistats.freemem = freeMem;
	seistats.uptime = uptime;
	seistats.pid = pid;
	seistats.bufferCount = encoderCount;
	seistats.sessionCount = sessionCount;
	seistats.meta = meta;
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
			mDebug("buffer size mismatch at FUI start: %d != %d", currentNal.size(), annexPrefix.size());
			h264FramingError();
			currentNal = annexPrefix;
		}
		if (start) {
			firstPacketTs = ts;
			currentNal.append(type | nri);
		} else if (currentNal.size() < annexPrefix.size() + 1)
			h264FramingError();
		currentNal.append(ba.mid(14));

		/* check end-last mismatch */
		if (end && !last) {
			ffDebug() << "end and last mismatch";
			h264FramingError();
		}
		if (!end && last) {
			ffDebug() << "last and end mismatch";
			h264FramingError();
		}

	} else
		currentNal.append(ba.mid(12));

	if (last) {
		/* try to extract frame-rate information from SPS */
		int nal = SimpleH264Parser::getNalType((const uchar *)currentNal.constData());
		if (expectedFrameRate < 0.001 && nal == SimpleH264Parser::NAL_SPS) {
			QByteArray spsbuf(currentNal.size(), ' ');
			SimpleH264Parser::escapeEmulation((uchar *)spsbuf.data(),
														  (const uchar *)currentNal.constData(), currentNal.size());
			SimpleH264Parser::sps_t sps = SimpleH264Parser::parseSps((const uchar *)spsbuf.constData());
			if (sps.vui.num_unit_in_ticks)
				expectedFrameRate = (float)sps.vui.timescale / sps.vui.num_unit_in_ticks / 2;
		}
		if (!containsMissing && !framingError) {
			/*
			 * Calculate frame time in terms device's wall clock.
			 *
			 * rtcpEpoch is the camera's NTP time at the time of RTCP
			 * report. rtcpTs is the corresponding RTP timestamp at
			 * that time.
			 */
			qint64 foph = 0;
			if (ts > stats.rtcpTs) {
				qint64 rtpDiff = ts - stats.rtcpTs;
				foph = stats.rtcpEpoch + rtpDiff * 1000 / 90000;
			} else {
				qint64 rtpDiff = stats.rtcpTs - ts;
				foph = stats.rtcpEpoch - rtpDiff * 1000 / 90000;
			}

			RawBuffer buf("application/x-rtp", currentNal.constData(), currentNal.size());
			buf.pars()->pts = ts;
			buf.pars()->bufferNo = validFrameCount++;
#if QT_VERSION >= 0x050000
			buf.pars()->captureTime = QDateTime::currentMSecsSinceEpoch();
#endif
			buf.pars()->encodeTime = foph;
			buf.pars()->streamBufferNo = buf.pars()->bufferNo;
			if (nal == SimpleH264Parser::NAL_SEI)
				parseSeiUserData(buf);
			getInputQueue(0)->addBuffer(buf, this);
		} else {
			validFrameCount++;
			int tdiffex = getRtpClock() / expectedFrameRate;
			int tdiff = qAbs(ts - firstPacketTs);
			int jump = qMax(1, tdiff / tdiffex);
			validFrameCount += jump;
			mDebug("Framing error, skipping %d frame(s)", jump);
		}
		framingError = false;
		containsMissing = false;
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
