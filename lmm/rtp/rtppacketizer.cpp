#include "rtppacketizer.h"

#include "debug.h"

#include <QUdpSocket>

#include <errno.h>

#define RTP_VERSION 2

RtpPacketizer::RtpPacketizer(QObject *parent) :
	BaseLmmElement(parent)
{
	maxPayloadSize = 1460;
	frameRate = 30.0;
}

Lmm::CodecType RtpPacketizer::codecType()
{
	return Lmm::CODEC_H264;
}

int RtpPacketizer::start()
{
	streamedBufferCount = 0;
	seq = 6666;
	ssrc = 0x6335D514;
	baseTs = 1578998879;
	sock = new QUdpSocket;
	sock->bind(srcDataPort);
	createSdp();
	return BaseLmmElement::start();
}

int RtpPacketizer::stop()
{
	sock->deleteLater();
	sock = NULL;
	return BaseLmmElement::stop();
}

static const uint8_t * findNextStartCodeIn(const uint8_t *p, const uint8_t *end)
{
	const uint8_t *a = p + 4 - ((intptr_t)p & 3);

	for (end -= 3; p < a && p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	for (end -= 3; p < end; p += 4) {
		uint32_t x = *(const uint32_t*)p;
		//      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
		//      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
		if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
			if (p[1] == 0) {
				if (p[0] == 0 && p[2] == 1)
					return p;
				if (p[2] == 0 && p[3] == 1)
					return p+1;
			}
			if (p[3] == 0) {
				if (p[2] == 0 && p[4] == 1)
					return p+2;
				if (p[4] == 0 && p[5] == 1)
					return p+3;
			}
		}
	}

	for (end += 3; p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	return end + 3;
}

static const uint8_t * findNextStartCode(const uint8_t *p, const uint8_t *end){
	const uint8_t *out= findNextStartCodeIn(p, end);
	if(p<out && out<end && !out[-1]) out--;
	return out;
}

int RtpPacketizer::sendNalUnit(const uchar *buf, int size)
{
	uchar rtpbuf[maxPayloadSize + 12];
	uchar type = buf[0] & 0x1F;
	uchar nri = buf[0] & 0x60;
	if (type >= 26) {
		mDebug("undefined nal type %d, not sending", type);
		return 0;
	}
	if (size <= maxPayloadSize) {
		memcpy(rtpbuf + 12, buf, size);
		sendRtpData(rtpbuf, size, 1);
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
			sendRtpData(rtpbuf, maxPayloadSize, 0);
			buf += maxPayloadSize - 2;
			size -= maxPayloadSize - 2;
			dstbuf[1] &= ~(1 << 7);
		}
		dstbuf[1] |= 1 << 6;
		memcpy(&dstbuf[2], buf, size);
		sendRtpData(rtpbuf, size + 2, 1);
	}
	return 0;
}

void RtpPacketizer::sendRtpData(uchar *buf, int size, int last)
{
	uint ts = baseTs + packetTimestamp();
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
	sock->writeDatagram((const char *)buf, size + 12, QHostAddress(dstIp), dstDataPort);
	seq++;
	//if (size < 10)
	mLog("sending %d bytes", size + 12);
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
	emit sdpReady(lines.join("\n"));
}

static void dump(const char *var, const uchar *d)
{
	qDebug() << var << d[0] << d[1] << d[2] << d[3];
}

int RtpPacketizer::processBuffer(RawBuffer buf)
{
	streamedBufferCount++;
	mInfo("streaming next packet: %d bytes", buf.size());

	const uchar *data = (const uchar *)buf.constData();
	int size = buf.size();
	const uchar *end = data + size;
	while (1) {
		const uchar *first = findNextStartCode(data, data + size);
		if (first >= end)
			break;
		const uchar *next = findNextStartCode(first + 4, first + size - 4);
		if (next >= end) {
			mDebug("unexpected end of buffer, ignoring");
			break;
		}
		if (end - next == 4)
			next = end;
		/* while sending we omit NAL start-code */
		sendNalUnit(first + 4, next - first - 4);
		data += next - first;
		size -= next - first;
		if (size <= 0 || data == end)
			break;
	}
	return 0;
}

int RtpPacketizer::packetTimestamp()
{
	return 90000ll * streamedBufferCount / frameRate;
}
