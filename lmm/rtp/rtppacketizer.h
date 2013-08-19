#ifndef RTPPACKETIZER_H
#define RTPPACKETIZER_H

#include <lmm/lmmcommon.h>
#include <lmm/baselmmelement.h>

class QUdpSocket;

class RtpPacketizer : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit RtpPacketizer(QObject *parent = 0);
	Lmm::CodecType codecType();
	virtual int start();
	virtual int stop();

	void setDestinationIpAddress(QString ip) { dstIp = ip; }
	void setRtpSequenceOffset(int value) { rtpSequenceOffset = value; }
	void setRtpTimestampOffset(int value) { rtpTimestampOffset = value; }
	void setDestinationDataPort(int port) { dstDataPort = port; }
	void setDestinationControlPort(int port) { dstControlPort = port; }
	void setSourceDataPort(int port) { srcDataPort = port; }
	void setSourceControlPort(int port) { srcControlPort = port; }
	int getSourceDataPort() { return srcDataPort; }
	int getSourceControlPort() { return srcControlPort; }
	QString getDestinationIpAddress() { return dstIp; }
	int getDestinationDataPort() { return dstDataPort; }
	int getDestinationControlPort() { return dstControlPort; }
	QString getSdp();
	void setFrameRate(float fps) { frameRate = fps; }
protected:
	virtual int processBuffer(RawBuffer buf);
	virtual int packetTimestamp();
	int sendNalUnit(const uchar *buf, int size);
	void sendRtpData(uchar *buf, int size, int last);
	int maxPayloadSize;
	int seq;
	uint ssrc;
	uint baseTs;
	QUdpSocket *sock;

	int dstDataPort;
	int dstControlPort;
	int srcDataPort;
	int srcControlPort;
	int rtpSequenceOffset;
	int rtpTimestampOffset;
	QString dstIp;
	float frameRate;
	int streamedBufferCount;
};

#endif // RTPPACKETIZER_H
