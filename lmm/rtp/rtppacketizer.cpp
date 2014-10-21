#include "rtppacketizer.h"
#include "h264parser.h"
#include "debug.h"
#include "tools/rawnetworksocket.h"
#include "streamtime.h"

#include <QUdpSocket>
#include <QElapsedTimer>
#include <QNetworkInterface>

#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define RTP_VERSION 2

static qint64 getCurrentTime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (qint64)tv.tv_sec * 1000000 + tv.tv_usec;
}

RtpPacketizer::RtpPacketizer(QObject *parent) :
	BaseLmmElement(parent)
{
	maxPayloadSize = 1460;
	tempRtpBuf = new uchar[maxPayloadSize + 12];
	frameRate = 30.0;
	packetized = true;
	passThru = false;
	zeroCopy = true;
	srcDataPort = 0;
	srcControlPort = 0;
	useStapA = true;
}

Lmm::CodecType RtpPacketizer::codecType()
{
	return Lmm::CODEC_H264;
}

int RtpPacketizer::start()
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
	quint32 ip = QHostAddress(dstIp).toIPv4Address();
	bool multicast = false;
	if (ip >> 28 == 0xe)
		multicast = true;
	sampleNtpRtp = true;
	streamedBufferCount = 0;
	seq = rand() % 4096;
	baseTs = rand();
	totalOctetCount = 0;
	totalPacketCount = 0;
	sock = new QUdpSocket;
	sock2 = new QUdpSocket;
	bitrateBufSize = 0;
	bitrate = 0;
	if (zeroCopy) {
		if (!initZeroCopy()) {
			mDebug("failed to initialize zero-copy, reverting to normal sockets");
			zeroCopy = false;
		}
	}
	if (!zeroCopy)
		sock->bind(srcDataPort);
	if (multicast && (srcControlPort != dstControlPort)) {
		mDebug("Source and destination control ports should be same for multicast, fixing it.");
		srcControlPort = dstControlPort;
	}
	sock2->bind(srcControlPort);
	connect(sock2, SIGNAL(readyRead()), SLOT(readPendingRtcpDatagrams()));
	if (multicast) {
		/* adjust multicast */
		ip_mreq mreq;
		memset(&mreq, 0, sizeof(ip_mreq));
		mreq.imr_multiaddr.s_addr = inet_addr(qPrintable(dstIp));
		mreq.imr_interface.s_addr = htons(INADDR_ANY);
		mDebug("joining multicast group %s", qPrintable(dstIp));
		int sd = 0;
		if (!zeroCopy)
			sd = sock->socketDescriptor();
		else
			sd = rawsock->socketDescriptor();
		if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
			qDebug() << "error joining multicast group";
		memset(&mreq, 0, sizeof(ip_mreq));
		mreq.imr_multiaddr.s_addr = inet_addr(qPrintable(dstIp));
		mreq.imr_interface.s_addr = htons(INADDR_ANY);
		if (setsockopt(sock2->socketDescriptor(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
			qDebug() << "error joining multicast group";
	}
	rtcpTime.start();
	createSdp();
	flush();
	return BaseLmmElement::start();
}

int RtpPacketizer::stop()
{
	bitrate = 0;
	streamLock.lock();
	sock->deleteLater();
	sock2->deleteLater();
	sock = NULL;
	sock2 = NULL;
	streamLock.unlock();
	return BaseLmmElement::stop();
}

void RtpPacketizer::sampleNtpTime()
{
	sampleNtpRtp = true;
}

QString RtpPacketizer::getSdp()
{
	return sdp;
}

int RtpPacketizer::sendNalUnit(const uchar *buf, int size, qint64 ts)
{
	RawNetworkSocket::SockBuffer *sbuf = NULL;
	uchar *rtpbuf;
	if (zeroCopy) {
		sbuf = rawsock->getNextFreeBuffer();
		rtpbuf = (uchar *)sbuf->payload;
	} else {
		rtpbuf = tempRtpBuf;
	}
	uchar type = buf[0] & 0x1F;
	uchar nri = buf[0] & 0x60;
	if (type >= 13) {
		mDebug("undefined nal type %d, not sending", type);
		return 0;
	}
	if (size <= maxPayloadSize) {
		memcpy(rtpbuf + 12, buf, size);
		sendRtpData(rtpbuf, size, 1, sbuf, ts);
	} else {
		uchar *dstbuf = rtpbuf + 12;
		dstbuf[0] = 28;        /* FU Indicator; Type = 28 ---> FU-A */
		dstbuf[0] |= nri;
		dstbuf[1] = type;
		dstbuf[1] |= 1 << 7;
		buf += 1;
		size -= 1;
		while (size + 2 > maxPayloadSize) {
			memcpy(&dstbuf[2], buf, maxPayloadSize - 2);
			sendRtpData(rtpbuf, maxPayloadSize, 0, sbuf, ts);
			buf += maxPayloadSize - 2;
			size -= maxPayloadSize - 2;
			dstbuf[1] &= ~(1 << 7);
		}
		dstbuf[1] |= 1 << 6;
		memcpy(&dstbuf[2], buf, size);
		sendRtpData(rtpbuf, size + 2, 1, sbuf, ts);
	}
	return 0;
}

void RtpPacketizer::sendRtpData(uchar *buf, int size, int last, void *sbuf, qint64 tsRef)
{
	uint ts = baseTs + tsRef;
	buf[0] = RTP_VERSION << 6;
	buf[1] = last << 7 | 96;
	buf[2] = seq >> 8;
	buf[3] = seq & 0xff;
	buf[4] = (ts >> 24) & 0xff;
	buf[5] = (ts >> 16) & 0xff;
	buf[6] = (ts >> 8) & 0xff;
	buf[7] = (ts >> 0) & 0xff;
	buf[8] = (ssrc >> 24) & 0xff;
	buf[9] = (ssrc >> 16) & 0xff;
	buf[10] = (ssrc >> 8) & 0xff;
	buf[11] = (ssrc >> 0) & 0xff;
	if (zeroCopy) {
		if (sbuf)
			rawsock->send((RawNetworkSocket::SockBuffer *)sbuf, size + 12);
		else
			rawsock->send((char *)buf, size + 12);
	} else
		sock->writeDatagram((const char *)buf, size + 12, QHostAddress(dstIp), dstDataPort);
	seq = (seq + 1) & 0xffff;
	totalPacketCount++;
	totalOctetCount += size;

	mLog("sending %d bytes", size + 12);
}

#define NTP_OFFSET 2208988800ULL  //ntp is from 1900 instead of 1970
void RtpPacketizer::sendSR()
{
	int length = 6; //in words, 7 - 1
	uchar buf[(length + 1) * 4];
	buf[0] = RTP_VERSION << 6;
	buf[1] = 200; //RTCP_SR
	buf[2] = length >> 8;
	buf[3] = length >> 0;
	buf[4] = (ssrc >> 24) & 0xff;
	buf[5] = (ssrc >> 16) & 0xff;
	buf[6] = (ssrc >> 8) & 0xff;
	buf[7] = (ssrc >> 0) & 0xff;

	/* ntp part */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	qint64 ntpu = (qint64)tv.tv_sec * 1000000 + tv.tv_usec + NTP_OFFSET * 1000000;
	uint ntps = tv.tv_sec + NTP_OFFSET;
	uint ntpf = (uint)((tv.tv_usec / 15625.0 ) * 0x04000000 + 0.5);

	buf[8] = (ntps >> 24) & 0xff;
	buf[9] = (ntps >> 16) & 0xff;
	buf[10] = (ntps >> 8) & 0xff;
	buf[11] = (ntps >> 0) & 0xff;
	buf[12] = (ntpf >> 24) & 0xff;
	buf[13] = (ntpf >> 16) & 0xff;
	buf[14] = (ntpf >> 8) & 0xff;
	buf[15] = (ntpf >> 0) & 0xff;

	uint rtpts = (ntpu - ntpRtpPair.first) * 90000 / 1000000 + ntpRtpPair.second;
	buf[16] = (rtpts >> 24) & 0xff;
	buf[17] = (rtpts >> 16) & 0xff;
	buf[18] = (rtpts >> 8) & 0xff;
	buf[19] = (rtpts >> 0) & 0xff;

	buf[20] = (totalPacketCount >> 24) & 0xff;
	buf[21] = (totalPacketCount >> 16) & 0xff;
	buf[22] = (totalPacketCount >> 8) & 0xff;
	buf[23] = (totalPacketCount >> 0) & 0xff;
	buf[24] = (totalOctetCount >> 24) & 0xff;
	buf[25] = (totalOctetCount >> 16) & 0xff;
	buf[26] = (totalOctetCount >> 8) & 0xff;
	buf[27] = (totalOctetCount >> 0) & 0xff;

	sock2->writeDatagram((const char *)buf, (length + 1) * 4, QHostAddress(dstIp), dstControlPort);
	rtcpTime.restart();
}

bool RtpPacketizer::initZeroCopy()
{
	rawsock = new RawNetworkSocket(dstIp, dstDataPort, myIpAddr.toString(), srcDataPort);
	return rawsock->isActive();
}

void RtpPacketizer::createSdp()
{
	QStringList lines;
	lines << "v=0";
	lines << "o=- 0 0 IN IP4 127.0.0.1";
	lines << "s=No Name";
	lines << QString("c=IN IP4 %1").arg(dstIp);
	lines << "t=0 0";
	lines << "a=tool:libavformat 52.102.0";
	lines << QString("m=video %1 RTP/AVP 96").arg(dstDataPort);
	lines << "a=rtpmap:96 H264/90000";
	lines << "a=fmtp:96 packetization-mode=1";
	sdp = lines.join("\n");
	emit sdpReady(sdp);
}

int RtpPacketizer::processBuffer(const RawBuffer &buf)
{
	mInfo("streaming next packet: %d bytes", buf.size());

	streamLock.lock();
	if (!sock) {
		streamLock.unlock();
		return -EINVAL;
	}

	/* check rtcp first */
	if (sampleNtpRtp) {
		ntpRtpPair.first = getCurrentTime() + NTP_OFFSET * 1000000;
		ntpRtpPair.second = baseTs + packetTimestamp(0);
		sampleNtpRtp = false;
		sendSR();
	}
	if (rtcpTime.elapsed() > 5000)
		sendSR();

	/* rtp part */
	const uchar *data = (const uchar *)buf.constData();
	int size = buf.size();
	const uchar *end = data + size;
	if (packetized) {
		if (int(data[4] & 0x1f) <= 5) {
			streamedBufferCount++;
		}
		sendNalUnit(data + 4, size - 4, buf.constPars()->encodeTime / 1000 * 90ull);
	} else {
		streamedBufferCount++;
		uchar *stapBuf = NULL;
		int stapSize = 0;
		uchar nriMax = 0;
		RawNetworkSocket::SockBuffer *sbuf = NULL;
		uchar *rtpbuf = NULL;
		qint64 ts = packetTimestamp(0);
		while (1) {
			const uchar *first = H264Parser::findNextStartCode(data, data + size);
			if (first >= end)
				break;
			const uchar *next = H264Parser::findNextStartCode(first + 4, first + size - 4);
			if (next >= end) {
				mDebug("unexpected end of buffer, ignoring");
				break;
			}
			if (end - next == 4)
				next = end;

			if (!useStapA) {
				/* while sending we omit NAL start-code */
				sendNalUnit(first + 4, next - first - 4, ts);
			} else {
				/* Use STAP-A messages */
				const uchar *buf = first + 4;
				int usize = next - first - 4;

				if (usize + 3 > maxPayloadSize) {
					mInfo("too big for stap-a(%d bytes), fuaing", usize);
					if (stapBuf) {
						mInfo("sending waiting stap-a(%d bytes)", stapSize);
						rtpbuf[12] |= nriMax;
						sendRtpData(rtpbuf, stapSize, 1, sbuf, packetTimestamp(0));
						stapSize = 0;
						nriMax = 0;
						stapBuf = NULL;
					}
					sendNalUnit(buf, usize, ts);
				} else {
					uchar type = buf[0] & 0x1F;
					uchar nri = buf[0] & 0x60;
					if (nri > nriMax)
						nriMax = nri;
					bool skip = false;
					if (type >= 13) {
						mDebug("undefined nal type %d", type);
						skip = true;
					}

					if (!skip) {
						if (!stapBuf) {
							if (zeroCopy) {
								sbuf = rawsock->getNextFreeBuffer();
								rtpbuf = (uchar *)sbuf->payload;
							} else {
								rtpbuf = tempRtpBuf;
							}
							stapBuf = rtpbuf + 12;
							stapBuf[0] = 24;        /* STAP-A */
							stapBuf++;
							stapSize++;
						}

						if (stapSize + usize + 2 <= maxPayloadSize) {
							mInfo("adding to stap-a nalu=%d(%d bytes)", type, usize);
							stapBuf[0] = usize >> 8;
							stapBuf[1] = usize & 0xff;
							memcpy(&stapBuf[2], buf, usize);
							stapBuf += usize + 2;
							stapSize += usize + 2;
						} else {
							mInfo("sending full stap-a(%d bytes)", stapSize);
							rtpbuf[12] |= nriMax;
							sendRtpData(rtpbuf, stapSize, 1, sbuf, packetTimestamp(0));
							stapSize = 0;
							nriMax = 0;
							stapBuf = NULL;
							continue;
						}
					}
				}
			}

			data += next - first;
			size -= next - first;
			if (size <= 0 || data == end)
				break;
		}
		if (stapBuf) {
			mInfo("sending waiting stap-a(%d bytes)", stapSize);
			rtpbuf[12] |= nriMax;
			sendRtpData(rtpbuf, stapSize, 1, sbuf, packetTimestamp(0));
			stapSize = 0;
			nriMax = 0;
			stapBuf = NULL;
		}
	}

	if (passThru)
		newOutputBuffer(0, buf);
	else {
		sentBufferCount++;
		calculateFps(buf);
	}

	streamLock.unlock();
	return 0;
}

quint64 RtpPacketizer::packetTimestamp(int stream)
{
	Q_UNUSED(stream);
	return 90ull * streamTime->getCurrentTimeMili();
}

void RtpPacketizer::calculateFps(const RawBuffer buf)
{
	fpsBufferCount++;
	bitrateBufSize += buf.size();
	if (fpsTiming->elapsed() > 1000) {
		bitrate = bitrateBufSize * 8 / 1000;
		int elapsed = fpsTiming->restart();
		elementFps = fpsBufferCount * 1000 / elapsed;
		fpsBufferCount = 0;
		bitrateBufSize = 0;
	}
}

void RtpPacketizer::readPendingRtcpDatagrams()
{
	streamLock.lock();
	while (sock2 && sock2->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(sock2->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		sock2->readDatagram(datagram.data(), datagram.size(),
								&sender, &senderPort);
		/* we may receive our own messages when using multicast */
		if (sender == myIpAddr)
			continue;
		/* TODO: check incoming message type */
		emit newReceiverReport(srcControlPort);
	}
	streamLock.unlock();
}
