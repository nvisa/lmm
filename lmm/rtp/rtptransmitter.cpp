#include "debug.h"
#include "rtptransmitter.h"
#include "tools/rawnetworksocket.h"
#include "streamtime.h"
#include "h264parser.h"
#include "tools/tokenbucket.h"
#include "platform_info.h"
#include "metrics.h"

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

static QHostAddress findIp(const QString &ifname)
{
	QHostAddress myIpAddr;
	/* Let's find our IP address */
	foreach (const QNetworkInterface iface, QNetworkInterface::allInterfaces()) {
		if (iface.name() != ifname)
			continue;
		if (iface.addressEntries().size()) {
			myIpAddr = iface.addressEntries().at(0).ip();
			break;
		}
	}
	return myIpAddr;
}

static QByteArray createSei(const char *payload, int psize, uchar ptype)
{
	int tsize = psize + 16;
	int i;
	QByteArray ba;
	ba.append((uchar)6);
	ba.append((uchar)ptype);
	for(i = 0; i <= tsize - 255; i += 255)
		ba.append((uchar)255);
	ba.append((uchar)(tsize - i));
	for (int i = 0; i < 16; i++)
		ba.append((uchar)0xAA);
	ba.append(payload, psize);
	ba.append((uchar)0x80); //RBSP
	return ba;
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
	forwardIncomingTimestamp = false;
	useIncomingTs = true;
	insertH264Sei = false;
	rtcpEnabled = true;
	tb = NULL;
	bufferCount = 0;
	mediaCodec = codec;
	useStapA = false;
	maxPayloadSize = 1460;
	ttl = 10;
	rtcpTimeoutValue = 60000;

	frameRate = 30.0;
	useAbsoluteTimestamp = true;

	/* Let's find our IP address */
	myIpAddr = findIp("eth0");

	tsinfo.enabled = false;
	tsinfo.avgBitsPerSec = 0;
	tsinfo.burstBitsPerSec = 0;
}

RtpChannel * RtpTransmitter::addChannel()
{
	int pt = 96;
	Lmm::CodecType codec = getCodec();
	if (codec == Lmm::CODEC_PCM_L16)
		pt = 97;
	else if (codec == Lmm::CODEC_PCM_ALAW)
		pt = 8;
	else if (codec == Lmm::CODEC_META_BILKON)
		pt = 98;
	else if (codec == Lmm::CODEC_JPEG)
		pt = 26;
	RtpChannel *ch = createChannel();
	ch->payloadType = pt;
	ch->ttl = ttl;
	ch->useSR = rtcpEnabled;
	ch->rtcpTimeoutValue = rtcpTimeoutValue;
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
	streamedBufferCount = 0;

	return BaseLmmElement::start();
}

int RtpTransmitter::stop()
{
	return BaseLmmElement::stop();
}

int RtpTransmitter::setupChannel(RtpChannel *ch, const QString &target, int dport, int dcport, int sport, int scport, uint ssrc)
{
	QMutexLocker l(&streamLock);
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

int RtpTransmitter::setTrafficShaping(bool enabled, int average, int burst, int duration)
{
	tsinfo.enabled = enabled;
	tsinfo.avgBitsPerSec = average;
	tsinfo.burstBitsPerSec = burst;
	tsinfo.controlDuration = duration;

	if (tsinfo.enabled) {
		tb = new TokenBucket(this);
		tb->setParsLeaky(tsinfo.avgBitsPerSec / 8, tsinfo.controlDuration);
	}

	return 0;
}

void RtpTransmitter::setRtcp(bool enabled)
{
	rtcpEnabled = enabled;
}

int RtpTransmitter::processBuffer(const RawBuffer &buf)
{
	incomingRtpTimestamp = buf.constPars()->pts;
	if (useIncomingTs && buf.constPars()->encodeTime)
		lastBufferTime = buf.constPars()->encodeTime / 1000;
	else
		lastBufferTime = streamTime->getCurrentTime() / 1000;
	streamLock.lock();
	/* check rtcp first */
	channelsCheckRtcp(packetTimestamp());
	Metrics::instance().outRtpDataBytesInc(buf.size());

	/* rtp part */
	if (getCodec() == Lmm::CODEC_H264)
		packetizeAndSend(buf);
	else if (getCodec() == Lmm::CODEC_PCM_L16 ||
			 getCodec() == Lmm::CODEC_PCM_ALAW)
		sendPcmData(buf);
	else if (getCodec() == Lmm::CODEC_META_BILKON)
		sendMetaData(buf);
	else if (getCodec() == Lmm::CODEC_JPEG)
		sendJpegData(buf);

	/* let's prevent too much of buffer accumulation */
	if (getInputQueue(0)->getTotalSize() > 1024 * 1024 * 2)
		getInputQueue(0)->clear();

	streamLock.unlock();
	streamedBufferCount++;
	return newOutputBuffer(0, buf);
}

quint64 RtpTransmitter::packetTimestamp()
{
	if (forwardIncomingTimestamp)
		return incomingRtpTimestamp;
	else if (mediaCodec == Lmm::CODEC_H264) {
		if (useAbsoluteTimestamp)
			return 90ull * lastBufferTime;
		return (90000ll * (streamedBufferCount) / (int)frameRate);
	} else if (mediaCodec == Lmm::CODEC_PCM_L16 || mediaCodec == Lmm::CODEC_PCM_ALAW) {
		if (useAbsoluteTimestamp)
			return 8ull * lastBufferTime;
		return (8000ll * streamedBufferCount / 50);
	} else if (mediaCodec == Lmm::CODEC_META_BILKON)
		return 90ull * lastBufferTime;
	else if (mediaCodec == Lmm::CODEC_JPEG)
		return 90ull * lastBufferTime;;
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
			/* sei insertion logic */
			const uchar *nalbuf = first + dOff;
			uchar type = nalbuf[0] & 0x1F;
			if (insertH264Sei && buf.constPars()->metaData.size()
					&&(type == SimpleH264Parser::NAL_SLICE || type == SimpleH264Parser::NAL_SLICE_IDR)) {
				QByteArray sei = createSei(buf.constPars()->metaData, buf.constPars()->metaData.size(), 5);
				channelsSendNal((const uchar *)sei.constData(), sei.size(), ts);
			}

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
	qint64 ts = packetTimestamp();
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

void RtpTransmitter::sendMetaData(const RawBuffer &buf)
{
	if (!channels.size())
		return;
	qint64 ts = packetTimestamp();
	for (int i = 0; i < channels.size(); i++)
		channels[i]->sendPcmData((const uchar *)buf.constData(), buf.size(), ts);
}

void RtpTransmitter::sendJpegData(const RawBuffer &buf)
{
	if (!channels.size())
		return;
	qint64 ts = packetTimestamp();
	for (int i = 0; i < channels.size(); i++)
		channels[i]->sendJpegData((const uchar *)buf.constData(), buf.size(), ts);
}

void RtpTransmitter::channelsSendNal(const uchar *buf, int size, qint64 ts)
{
	/* shaping is active, we should split NAL before passing to channels */
	if (!channels.size())
		return;
	uchar *rtpbuf = channels.first()->tempRtpBuf;
	RawNetworkSocket::SockBuffer *sbuf = NULL;

	uchar type = buf[0] & 0x1F;
	uchar nri = buf[0] & 0x60;
	if (!bufferCount) {
		if (type != 7) //wait sps
			return;
	}
	bufferCount++;
	if (type >= 13) {
		mDebug("undefined nal type %d, not sending", type);
		return;
	}
	if (size <= maxPayloadSize) {
		memcpy(rtpbuf + 12, buf, size);
		channelsSendRtp(rtpbuf, size, 1, sbuf, ts);
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
			channelsSendRtp(rtpbuf, maxPayloadSize, 0, sbuf, ts);
			buf += maxPayloadSize - 2;
			size -= maxPayloadSize - 2;
			dstbuf[1] &= ~(1 << 7);
		}
		dstbuf[1] |= 1 << 6;
		memcpy(&dstbuf[2], buf, size);
		channelsSendRtp(rtpbuf, size + 2, 1, sbuf, ts);
	}
}

void RtpTransmitter::channelsSendRtp(uchar *buf, int size, int last, void *sbuf, qint64 tsRef)
{
	if (tb)
		tb->get(size + 12);
	/*
	 * only first RTP channel can benefit from exteme-zero copying,
	 * others should live with one extra memcpy
	 */
	if (channels.size())
		channels.first()->sendRtpData(buf, size, last, sbuf, tsRef);
	for (int i = 1; i < channels.size(); i++)
		channels[i]->sendRtpData(buf, size, last, NULL, tsRef);
}

void RtpTransmitter::channelsCheckRtcp(quint64 ts)
{
	for (int i = 0; i < channels.size(); i++)
		if (channels[i]->rtcpTime->elapsed() > 5000)
			channels[i]->sendSR(ts);
}

RtpChannel *RtpTransmitter::createChannel()
{
	return new RtpChannel(maxPayloadSize, myIpAddr);
}

RtpChannel::RtpChannel(int psize, const QHostAddress &myIpAddr)
{
	rtcpTimeoutValue = 60000;
	useSR = true;
	state = 0;
	this->myIpAddr = myIpAddr;
	totalOctetCount = 0;
	totalPacketCount = 0;
	maxPayloadSize = psize;
	rawsock = NULL;
	seqFirst = seq = rand() % 4096;
	rtcpTs = 0;
	ntpEpoch = 0;
	baseTs = rand();
	sock = new QUdpSocket;
	sock2 = new QUdpSocket;
	/* zero copy is critical for performance on armv5, other archs can live without it */
	if (cpu_is_armv5())
		zeroCopy = true;
	else
		zeroCopy = false;
	tempRtpBuf = new uchar[maxPayloadSize + 12];
	rrTime = new MyTime;
	rtcpTime = new MyTime;
	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
	timer->start(1000);
	bufferCount = 0;
	tb = NULL;
	trHook = NULL;
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
	Metrics::instance().outRtpChannelCountDec();
	quint32 ip = QHostAddress(dstIp).toIPv4Address();
	if (ip >> 28 == 0xe)
		Metrics::instance().outRtpMulticastChannelCountDec();
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
		if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
			ffDebug() << "error setting multicast TTL value for RTP socket";
		if (setsockopt(sock2->socketDescriptor(), IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
			ffDebug() << "error setting TTL value for RTCP socket";
		if (setsockopt(sock2->socketDescriptor(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
			ffDebug() << "error setting multicast TTL value for RTCP socket";
		memset(&mreq, 0, sizeof(ip_mreq));
		mreq.imr_multiaddr.s_addr = inet_addr(qPrintable(dstIp));
		mreq.imr_interface.s_addr = htons(INADDR_ANY);
		if (setsockopt(sock2->socketDescriptor(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
			ffDebug() << "error joining multicast group";
		Metrics::instance().outRtpMulticastChannelCountInc();
	}
	rtcpTime->start();
	state = 1;
	rrTime->start();
	Metrics::instance().outRtpChannelCountInc();

	return 0;
}

int RtpChannel::play()
{
	state = 2;
	rrTime->restart();
	Metrics::instance().outVideoStreamsInc();
	Metrics::instance().outRtpPlayingChannelCountInc();
	return 0;
}

int RtpChannel::teardown()
{
	state = 0;
	Metrics::instance().outRtpPlayingChannelCountDec();
	Metrics::instance().outVideoStreamsDec();
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

int RtpChannel::sendJpegData(const uchar *buf, int size, qint64 ts)
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

	uchar *jpegHeader = rtpbuf + 12;
	jpegHeader[0] = 0;			/* type-specific */
	jpegHeader[1] = 0;
	jpegHeader[2] = 0;
	jpegHeader[3] = 0;
	jpegHeader[4] = 64;			/* type, DM365 uses 4:2:0 */
	jpegHeader[5] = 255;		/* Q */
	jpegHeader[6] = 640 / 8;	/* width */
	jpegHeader[7] = 368 / 8;	/* height */
	int jpegFixHeaderSize = 8;

	/* restart interval */
	jpegHeader[8] = 0;
	jpegHeader[9] = 84;
	jpegHeader[10] = 0xff;
	jpegHeader[11] = 0xff;
	jpegFixHeaderSize += 4;

	/* we will skip restart markers */
	uchar *qth = jpegHeader + jpegFixHeaderSize;
	qth[0] = 0;
	qth[1] = 0;
	qth[2] = 0;
	qth[3] = 128;

	/*
	 * JFIF: Jpeg File Interchange Format
	 *
	 * A JFIF consists segments of:
	 *	FF xx s1 s2
	 *
	 * Segments:
	 *	D8 -> Start Of Image (SOI)
	 *	E0 -> APP0
	 *
	 *	D0 -> Restart (RSTn), in the range [D0 D7]
	 *	D9 -> End Of Image (EOI)
	 *	DA -> Start of Scan (SOS)
	 *	DB -> Define Quantization Table (DQT)
	 *	DD -> Define Restart Interval (DRI)
	 *
	 *  C0 -> Start Of Frame (SOF)
	 *	C4 -> Define Huffman Table
	 *
	 * Now we do as ffmpeg does:
	 *	1. Copy DQT to RTP header
	 *	1. Skip up to and including SOS
	 */

	const uchar *qtables = NULL;
	int sosOff = 0;

	for (int i = 0; i < size; i++) {
		if (buf[i] != 0xff)
			continue;
		int seqsize = buf[i + 2] * 256 + buf[i + 3] + 2;
		int marker = buf[i + 1];
		if (marker == 0xdb)
			qtables = &buf[i + 4];
		if (marker == 0xda) {
			sosOff = i + seqsize;
			break;
		}
	}

	size -= sosOff;
	buf += sosOff;

	for (int i = size - 2; i >= 0; i--) {
		if (buf[i] == 0xff && buf[i + 1] == 0xd9) {
			size = i;
			break;
		}
	}

	int sentTotal = 0;
	while (size > 0) {
		int hsize = jpegFixHeaderSize;
		if (sentTotal == 0)
			hsize += 132;
		int len = qMin(size, maxPayloadSize - hsize);
		uchar *jpegbuf = rtpbuf + 12;

		/* adjust header */
		jpegHeader[1] = (sentTotal >> 16) & 0xff;
		jpegHeader[2] = (sentTotal >> 8) & 0xff;
		jpegHeader[3] = (sentTotal >> 0) & 0xff;
		if (sentTotal == 0 && qtables)
			memcpy(jpegbuf + jpegFixHeaderSize + 4, qtables, 128);
		/* payload */
		memcpy(jpegbuf + hsize, buf, len);
		sendRtpData(rtpbuf, len + hsize, len == size, sbuf, ts);

		buf += len;
		size -= len;
		sentTotal += len;
	}
	return 0;
}

void RtpChannel::sendRtpData(uchar *buf, int size, int last, void *sbuf, qint64 tsRef)
{
	if (state != 2)
		return;

	if (tb)
		tb->get(size + 12);

	uint ts = baseTs + tsRef;
	lastRtpTs = ts;
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
	if (trHook) {
		(*trHook)((const char *)buf, size + 12, this, trHookPriv);
	} else if (zeroCopy) {
		if (sbuf)
			rawsock->send((RawNetworkSocket::SockBuffer *)sbuf, size + 12);
		else
			rawsock->send((char *)buf, size + 12);
	} else
		writeSockData((const char *)buf, size + 12);
	seq = (seq + 1) & 0xffff;
	totalPacketCount++;
	totalOctetCount += size;
	Metrics::instance().outRtpPacketsInc();
	Metrics::instance().outRtpBytesInc(size + 12);

	mLog("sending %d bytes", size + 12);
}

void RtpChannel::sendSR(quint64 bufferTs)
{
	if (!useSR)
		return;
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
	uint ntps = tv.tv_sec + NTP_OFFSET;
	uint ntpf = (uint)((tv.tv_usec / 15625.0 ) * 0x04000000 + 0.5);
	qint64 timet = (qint64)tv.tv_sec * 1000 + tv.tv_usec / 1000;
	uint currTs = baseTs + (uint)(bufferTs & 0xffffffff);
	if (!rtcpTs || ntpEpoch > timet) {
		rtcpTs = currTs;
		ntpEpoch = timet;
	}
	buf[8] = (ntps >> 24) & 0xff;
	buf[9] = (ntps >> 16) & 0xff;
	buf[10] = (ntps >> 8) & 0xff;
	buf[11] = (ntps >> 0) & 0xff;
	buf[12] = (ntpf >> 24) & 0xff;
	buf[13] = (ntpf >> 16) & 0xff;
	buf[14] = (ntpf >> 8) & 0xff;
	buf[15] = (ntpf >> 0) & 0xff;

	uint rtpts = (timet - ntpEpoch) * 90 + rtcpTs;
	buf[16] = (rtpts >> 24) & 0xff;
	buf[17] = (rtpts >> 16) & 0xff;
	buf[18] = (rtpts >> 8) & 0xff;
	buf[19] = (rtpts >> 0) & 0xff;

	/*
	 * timestamp drift prevention
	 */
	uint diff = rtpts - currTs;
	if (currTs > rtpts)
		diff = currTs - rtpts;
	if (diff > 300) {
		/* don't let 2 timestamps get-out-of sync */
		rtcpTs = currTs;
		ntpEpoch = timet;
	}

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
	Metrics::instance().outRtpPacketsSRInc();
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
		sdp << QString("m=audio %1 RTP/AVP 8").arg(dstDataPort);
		sdp << "a=rtpmap:8 PCMA/8000/1";
	}  else if (codec == Lmm::CODEC_META_BILKON) {
		sdp << QString("m=metadata %1 RTP/AVP 98").arg(dstDataPort);
		sdp << "a=rtpmap:98 BILKON";
	}

	sdp << "";
	return sdp.join("\n");
}

void RtpChannel::timeout()
{
	if (state == 0)
		return;
	if (rrTime->elapsed() > rtcpTimeoutValue) {
		Metrics::instance().outRtpTimeoutInc();
		emit sessionTimedOut();
	}
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

void RtpChannel::writeSockData(const char *buf, int size)
{
	sock->writeDatagram(buf, size, QHostAddress(dstIp), dstDataPort);
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
		Metrics::instance().outRtpPacketsRRInc();
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
	rawsock = new RawNetworkSocket(dstIp, dstDataPort, myIpAddr.toString(), srcDataPort, ttl);
	return rawsock->isActive();
}
