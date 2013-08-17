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
	return BaseLmmElement::start();
}

int RtpPacketizer::stop()
{
	sock->deleteLater();
	sock = NULL;
	return BaseLmmElement::stop();
}

int RtpPacketizer::streamBlocking()
{
	if (!acquireInputSem(0))
		return -EINVAL;
	return stream();
}

int RtpPacketizer::sendNalUnit(const uchar *buf, int size)
{
	uchar rtpbuf[maxPayloadSize + 12];
	if (size <= maxPayloadSize) {
		memcpy(rtpbuf + 12, buf, size);
		sendRtpData(rtpbuf, size, 1);
	} else {
		uchar type = buf[0] & 0x1F;
		uchar nri = buf[0] & 0x60;
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
}

int RtpPacketizer::stream()
{
	inputLock.lock();
	if (inputBuffers.count() == 0) {
		inputLock.unlock();
		return -ENOENT;
	}
	RawBuffer buf = inputBuffers.takeFirst();
	inputLock.unlock();
	streamedBufferCount++;
	mDebug("streaming next packet");
	return sendNalUnit((const uchar *)buf.constData(), buf.size());
}

int RtpPacketizer::packetTimestamp()
{
	return 90000ll * streamedBufferCount / frameRate;
}
