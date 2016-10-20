#include "debug.h"
#include "rtptransmitter.h"
#include "tools/rawnetworksocket.h"
#include "streamtime.h"
#include "h264parser.h"

#include <QTimer>
#include <QUdpSocket>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QNetworkInterface>

#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define RTP_VERSION 2
#define NTP_OFFSET 2208988800ULL  //ntp is from 1900 instead of 1970

static inline ushort get16(const uchar *data)
{
	return (data[0] << 8) | data[1];
}

static inline uint get32(const uchar *data)
{
	return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static qint64 getCurrentTime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (qint64)tv.tv_sec * 1000000 + tv.tv_usec;
}

class MyTime
{
public:
	MyTime()
	{

	}
	int elapsed()
	{
		l.lock();
		int el = t.elapsed();
		l.unlock();
		return el;
	}
	int restart()
	{
		l.lock();
		int el = t.restart();
		l.unlock();
		return el;
	}
	void start()
	{
		l.lock();
		t.start();
		l.unlock();
	}
protected:
	QElapsedTimer t;
	QMutex l;
};

RtpTransmitter::RtpTransmitter(QObject *parent, Lmm::CodecType codec) :
	BaseLmmElement(parent)
{
	mediaCodec = codec;
	useStapA = false;
	maxPayloadSize = 1460;
	sampleNtpRtp = false;
	ttl = 10;

	frameRate = 30.0;
	useAbsoluteTimestamp = true;

	/* Let's find our IP address */
	foreach (const QNetworkInterface iface, QNetworkInterface::allInterfaces()) {
		if (iface.name() != "eth0")
			continue;
		if (iface.addressEntries().size()) {
			myIpAddr = iface.addressEntries().at(0).ip();
			break;
		}
	}
}

void RtpTransmitter::sampleNtpTime()
{
	sampleNtpRtp = true;
}

RtpChannel * RtpTransmitter::addChannel()
{
	int pt = 96;
	Lmm::CodecType codec = getCodec();
	if (codec == Lmm::CODEC_PCM_L16
			|| codec == Lmm::CODEC_PCM_ALAW)
		pt = 97;
	RtpChannel *ch = new RtpChannel(maxPayloadSize, myIpAddr);
	ch->payloadType = pt;
	ch->ttl = ttl;
	QMutexLocker l(&streamLock);
	channels << ch;
	return ch;
}

int RtpTransmitter::getChannelCount()
{
	QMutexLocker l(&streamLock);
	return channels.size();
}

RtpChannel * RtpTransmitter::getChannel(int ch)
{
	QMutexLocker l(&streamLock);
	if (ch >= channels.size())
		return NULL;
	return channels[ch];
}

void RtpTransmitter::removeChannel(RtpChannel *ch)
{
	QMutexLocker l(&streamLock);
	channels.removeAll(ch);
	ch->deleteLater();
}

Lmm::CodecType RtpTransmitter::getCodec()
{
	return mediaCodec;
}

int RtpTransmitter::start()
{
	sampleNtpRtp = true;
	streamedBufferCount = 0;
	bitrateBufSize = 0;
	bitrate = 0;

	return BaseLmmElement::start();
}

int RtpTransmitter::stop()
{
	bitrate = 0;
	return BaseLmmElement::stop();
}

int RtpTransmitter::setupChannel(RtpChannel *ch, const QString &target, int dport, int dcport, int sport, int scport, uint ssrc)
{
	QMutexLocker l(&streamLock);
	ch->ntpRtpPair = ntpRtpPair;
	return ch->setup(target, dport, dcport, sport, scport, ssrc);
}

int RtpTransmitter::playChannel(RtpChannel *ch)
{
	QMutexLocker l(&streamLock);
	return ch->play();
}

int RtpTransmitter::teardownChannel(RtpChannel *ch)
{
	QMutexLocker l(&streamLock);
	return ch->teardown();
}

void RtpTransmitter::setMulticastTTL(socklen_t ttl)
{
	this->ttl = ttl;
}

void RtpTransmitter::setMaximumPayloadSize(int value)
{
	maxPayloadSize = value;
}

bool RtpTransmitter::isActive()
{
	return getChannelCount() ? true : false;
}

int RtpTransmitter::processBuffer(const RawBuffer &buf)
{
	lastBufferTime = buf.constPars()->encodeTime / 1000;
	streamLock.lock();
	/* check rtcp first */
	if (sampleNtpRtp) {
		qint64 t = getCurrentTime() + NTP_OFFSET * 1000000;
		qint64 pts = packetTimestamp();
		ntpRtpPair.first = t;
		ntpRtpPair.second = pts;
		channelsSetTimestamp(t, pts);
		sampleNtpRtp = false;
	}
	channelsCheckRtcp();

	/* rtp part */
	if (getCodec() == Lmm::CODEC_H264)
		packetizeAndSend(buf);
	else if (getCodec() == Lmm::CODEC_PCM_L16 ||
			 getCodec() == Lmm::CODEC_PCM_ALAW)
		sendPcmData(buf);

	streamLock.unlock();
	streamedBufferCount++;
	return newOutputBuffer(0, buf);
}

quint64 RtpTransmitter::packetTimestamp()
{
	if (mediaCodec == Lmm::CODEC_H264) {
		if (useAbsoluteTimestamp)
			return 90ull * lastBufferTime;
		return (90000ll * (streamedBufferCount) / (int)frameRate);
	} else if (mediaCodec == Lmm::CODEC_PCM_L16 || mediaCodec == Lmm::CODEC_PCM_ALAW) {
		if (useAbsoluteTimestamp)
			return 8ull * streamTime->getCurrentTimeMili();
		return (8000ll * streamedBufferCount / 50);
	}
	return 0;
}

void RtpTransmitter::packetizeAndSend(const RawBuffer &buf)
{
	if (!channels.size())
		return;
	const uchar *data = (const uchar *)buf.constData();
	int size = buf.size();
	const uchar *end = data + size;
	uchar *stapBuf = NULL;
	int stapSize = 0;
	uchar nriMax = 0;
	RawNetworkSocket::SockBuffer *sbuf = NULL;
	uchar *rtpbuf = NULL;
	qint64 ts = packetTimestamp();
	while (1) {
		const uchar *first = SimpleH264Parser::findNextStartCode(data, data + size);
		if (first >= end)
			break;
		const uchar *next = SimpleH264Parser::findNextStartCode(first + 4, first + size - 4);
		if (next >= end) {
			mDebug("unexpected end of buffer, ignoring");
			break;
		}
		if (end - next == 4)
			next = end;

		int dOff = 4;
		if (first[2] == 1)
			dOff = 3;

		if (!useStapA) {
			/* while sending we omit NAL start-code */
			channelsSendNal(first + dOff, next - first - dOff, ts);
		} else {
			/* Use STAP-A messages */
			const uchar *buf = first + dOff;
			int usize = next - first - dOff;

			if (usize + 3 > maxPayloadSize) {
				mInfo("too big for stap-a(%d bytes), fuaing", usize);
				if (stapBuf) {
					mInfo("sending waiting stap-a(%d bytes)", stapSize);
					rtpbuf[12] |= nriMax;
					channelsSendRtp(rtpbuf, stapSize, 1, sbuf, packetTimestamp());
					stapSize = 0;
					nriMax = 0;
					stapBuf = NULL;
				}
				channelsSendNal(buf, usize, ts);
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
						sbuf = channels.first()->getSBuf();
						rtpbuf = channels.first()->getRtpBuf(sbuf);
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
						channelsSendRtp(rtpbuf, stapSize, 1, sbuf, packetTimestamp());
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
		channelsSendRtp(rtpbuf, stapSize, 1, sbuf, packetTimestamp());
		stapSize = 0;
		nriMax = 0;
		stapBuf = NULL;
	}
}

void RtpTransmitter::sendPcmData(const RawBuffer &buf)
{
	if (!channels.size())
		return;
	int tsScaler = 1;
	if (getCodec() == Lmm::CODEC_PCM_L16)
		tsScaler = 2;
	else if (getCodec() == Lmm::CODEC_PCM_ALAW)
		tsScaler = 1;
	qint64 ts = streamedBufferCount * buf.size() / tsScaler;
	for (int i = 0; i < channels.size(); i++) {
		const uchar *data = (const uchar *)buf.constData();
		int size = buf.size();
		while (size > maxPayloadSize) {
			channels[i]->sendPcmData(data, maxPayloadSize, ts);
			data += maxPayloadSize;
			size -= maxPayloadSize;
			ts += maxPayloadSize / tsScaler;
		}
		channels[i]->sendPcmData(data, size, ts);
		//channels[i]->sendPcmData((const uchar *)buf.constData(), buf.size(), packetTimestamp());
	}
}

void RtpTransmitter::channelsSetTimestamp(qint64 current, qint64 packet)
{
	for (int i = 0; i < channels.size(); i++) {
		channels[i]->ntpRtpPair.first = current;
		channels[i]->ntpRtpPair.second = packet;
		channels[i]->sendSR();
	}
}

void RtpTransmitter::channelsSendNal(const uchar *buf, int size, qint64 ts)
{
	for (int i = 0; i < channels.size(); i++)
		channels[i]->sendNalUnit(buf, size, ts);
}

void RtpTransmitter::channelsSendRtp(uchar *buf, int size, int last, void *sbuf, qint64 tsRef)
{
	/*
	 * only first RTP channel can benefit from exteme-zero copying,
	 * others should live with one extra memcpy
	 */
	if (channels.size())
		channels.first()->sendRtpData(buf, size, last, sbuf, tsRef);
	for (int i = 1; i < channels.size(); i++)
		channels[i]->sendRtpData(buf, size, last, NULL, tsRef);
}

void RtpTransmitter::channelsCheckRtcp()
{
	for (int i = 0; i < channels.size(); i++)
		if (channels[i]->rtcpTime->elapsed() > 5000)
			channels[i]->sendSR();
}

RtpChannel::RtpChannel(int psize, const QHostAddress &myIpAddr)
{
	state = 0;
	this->myIpAddr = myIpAddr;
	totalOctetCount = 0;
	totalPacketCount = 0;
	maxPayloadSize = psize;
	rawsock = NULL;
	seq = rand() % 4096;
	baseTs = rand();
	sock = new QUdpSocket;
	sock2 = new QUdpSocket;
	zeroCopy = true;
	tempRtpBuf = new uchar[maxPayloadSize + 12];
	rrTime = new MyTime;
	rtcpTime = new MyTime;
	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
	timer->start(1000);
	bufferCount = 0;
}

RtpChannel::~RtpChannel()
{
	sock->deleteLater();
	sock2->deleteLater();
	delete tempRtpBuf;
	delete rrTime;
	delete rtcpTime;
	timer->stop();
	timer->deleteLater();
	if (rawsock)
		delete rawsock;
}

int RtpChannel::setup(const QString &target, int dport, int dcport, int sport, int scport, uint ssrc)
{
	dstDataPort = dport;
	dstControlPort = dcport;
	srcDataPort = sport;
	srcControlPort = scport;
	this->ssrc = ssrc;

	dstIp = target;
	quint32 ip = QHostAddress(dstIp).toIPv4Address();
	bool multicast = false;
	if (ip >> 28 == 0xe)
		multicast = true;

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
			ffDebug() << "error joining multicast group";
		if (setsockopt(sd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
			ffDebug() << "error setting TTL value for RTP socket";
		if (setsockopt(sock2->socketDescriptor(), IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
			ffDebug() << "error setting TTL value for RTCP socket";
		memset(&mreq, 0, sizeof(ip_mreq));
		mreq.imr_multiaddr.s_addr = inet_addr(qPrintable(dstIp));
		mreq.imr_interface.s_addr = htons(INADDR_ANY);
		if (setsockopt(sock2->socketDescriptor(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
			ffDebug() << "error joining multicast group";
	}
	rtcpTime->start();
	state = 1;
	rrTime->start();

	return 0;
}

int RtpChannel::play()
{
	state = 2;
	rrTime->restart();
	return 0;
}

int RtpChannel::teardown()
{
	state = 0;
	return 0;
}

int RtpChannel::sendNalUnit(const uchar *buf, int size, qint64 ts)
{
	if (state != 2)
		return 0;
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
	if (!bufferCount) {
		if (type != 7) //wait sps
			return 0;
	}
	bufferCount++;
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

int RtpChannel::sendPcmData(const uchar *buf, int size, qint64 ts)
{
	if (state != 2)
		return 0;
	RawNetworkSocket::SockBuffer *sbuf = NULL;
	uchar *rtpbuf;
	if (zeroCopy) {
		sbuf = rawsock->getNextFreeBuffer();
		rtpbuf = (uchar *)sbuf->payload;
	} else {
		rtpbuf = tempRtpBuf;
	}
	bufferCount++;

	if (size <= maxPayloadSize) {
		memcpy(rtpbuf + 12, buf, size);
		sendRtpData(rtpbuf, size, 1, sbuf, ts);
	} else {
		uchar *dstbuf = rtpbuf + 12;
		while (size > maxPayloadSize) {
			memcpy(&dstbuf[0], buf, maxPayloadSize);
			sendRtpData(rtpbuf, maxPayloadSize, 0, sbuf, ts);
			buf += maxPayloadSize;
			size -= maxPayloadSize;
		}
		memcpy(&dstbuf[0], buf, size);
		sendRtpData(rtpbuf, size, 1, sbuf, ts);
	}
	return 0;
}

void RtpChannel::sendRtpData(uchar *buf, int size, int last, void *sbuf, qint64 tsRef)
{
	if (state != 2)
		return;
	uint ts = baseTs + tsRef;
	buf[0] = RTP_VERSION << 6;
	buf[1] = last << 7 | payloadType;
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

void RtpChannel::sendSR()
{
	if (state != 2)
		return;
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

	uint rtpts = baseTs + (ntpu - ntpRtpPair.first) * 90000 / 1000000 + ntpRtpPair.second;
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

	sockLock.lock();
	sock2->writeDatagram((const char *)buf, (length + 1) * 4, QHostAddress(dstIp), dstControlPort);
	sockLock.unlock();
	rtcpTime->restart();
}

QString RtpChannel::getSdp(Lmm::CodecType codec)
{
	QStringList sdp;

	sdp << "v=0";
	sdp << "o=- 0 0 IN IP4 127.0.0.1";
	sdp << "c=IN IP4 " + dstIp;
	sdp << "a=tool:lmm 2.0.0";
	sdp << "s=No Name";
	sdp << "t=0 0";

	if (codec == Lmm::CODEC_H264) {
		sdp << QString("m=video %1 RTP/AVP 96").arg(dstDataPort);
		sdp << "a=rtpmap:96 H264/90000";
		sdp << "a=fmtp:96 packetization-mode=1";
	} else if (codec == Lmm::CODEC_JPEG) {
		sdp << "m=video 15678 RTP/AVP 26";
	} else if (codec == Lmm::CODEC_PCM_L16) {
		sdp << QString("m=audio %1 RTP/AVP 97").arg(dstDataPort);
		sdp << "a=rtpmap:97 L16/8000/1";
	} else if (codec == Lmm::CODEC_PCM_ALAW) {
		sdp << QString("m=audio %1 RTP/AVP 97").arg(dstDataPort);
		sdp << "a=rtpmap:97 PCMA/8000/1";
	}

	sdp << "";
	return sdp.join("\n");
}

void RtpChannel::timeout()
{
	if (state == 0)
		return;
	if (rrTime->elapsed() > 60000)
		emit sessionTimedOut();
}

RawNetworkSocket::SockBuffer *RtpChannel::getSBuf()
{
	if (zeroCopy)
		return rawsock->getNextFreeBuffer();
	return NULL;
}

uchar *RtpChannel::getRtpBuf(RawNetworkSocket::SockBuffer *sbuf)
{
	if (zeroCopy)
		return (uchar *)sbuf->payload;
	return tempRtpBuf;
}

void RtpChannel::readPendingRtcpDatagrams()
{
	int flags = 0;
	sockLock.lock();
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
#if 0
		const uchar *data = (const uchar *)datagram.data();
		uint left = datagram.size();
		while (left > 0) {
			if (data[1] == 201)
				flags |= 0x1;
			else if (data[1] == 203)
				flags |= 0x2;
			else
				break; /* un-supported message packet */
			ushort length = get16(&data[2]);
			uint ssrc = get32(&data[4]);
			Q_UNUSED(ssrc);
			left -= (length + 1) * 4;
			data += (length + 1) * 4;
		}
#else
		rrTime->restart();
#endif
	}
	sockLock.unlock();
	if (flags & 0x1)
		rrTime->restart();
	if (flags & 0x2)
		emit goodbyeRecved();
}

bool RtpChannel::initZeroCopy()
{
	if (rawsock)
		delete rawsock;
	rawsock = new RawNetworkSocket(dstIp, dstDataPort, myIpAddr.toString(), srcDataPort);
	return rawsock->isActive();
}
