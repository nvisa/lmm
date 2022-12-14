#ifndef RTPPACKETIZER_H
#define RTPPACKETIZER_H

#include <lmm/lmmcommon.h>
#include <lmm/baselmmelement.h>

#include <QHostAddress>
#include <QElapsedTimer>

class QUdpSocket;
class RawNetworkSocket;

class RtpPacketizer : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit RtpPacketizer(QObject *parent = 0);
	Lmm::CodecType codecType();
	virtual int start();
	virtual int stop();

	void sampleNtpTime();
	void setDestinationIpAddress(QString ip) { dstIp = ip; }
	void setRtpSequenceOffset(int value) { rtpSequenceOffset = value; }
	void setRtpTimestampOffset(int value) { rtpTimestampOffset = value; }
	void setDestinationDataPort(int port) { dstDataPort = port; }
	void setDestinationControlPort(int port) { dstControlPort = port; }
	void setSourceDataPort(int port) { srcDataPort = port; }
	void setSourceControlPort(int port) { srcControlPort = port; }
	void setSsrc(uint val) { ssrc = val; }
	int getSourceDataPort() { return srcDataPort; }
	int getSourceControlPort() { return srcControlPort; }
	QString getDestinationIpAddress() { return dstIp; }
	int getDestinationDataPort() { return dstDataPort; }
	int getDestinationControlPort() { return dstControlPort; }
	QString getSdp();
	void setFrameRate(float fps) { frameRate = fps; }
	int getBitrate() { return bitrate; }
	uint getBaseTimestamp() { return baseTs; }
	uint getBaseSequence() { return seq; }

	bool isPacketized() { return packetized; }
	void setPacketized(bool v) { packetized = v; }
	void setAbsoluteTimestamp(bool v) { useAbsoluteTimestamp = v; }
	bool isUsingAbsoluteTimestamp() { return useAbsoluteTimestamp; }
signals:
	void sdpReady(QString sdp);
	void newReceiverReport(int);
protected slots:
	void readPendingRtcpDatagrams();
protected:
	virtual int processBuffer(const RawBuffer &buf);
	virtual quint64 packetTimestamp(int stream);
	int sendNalUnit(const uchar *buf, int size, qint64 ts);
	void sendRtpData(uchar *buf, int size, int last, void *sbuf, qint64 tsRef);
	void createSdp();
	void sendSR();
	bool initZeroCopy();

	int maxPayloadSize;
	int seq;
	uint ssrc;
	uint baseTs;
	QUdpSocket *sock;
	QUdpSocket *sock2; //rtcp socket
	QMutex streamLock;
	bool packetized;
	QPair<qint64, uint> ntpRtpPair;
	QElapsedTimer rtcpTime;
	QHostAddress myIpAddr;
	uchar *tempRtpBuf;
	bool useStapA;
	bool useAbsoluteTimestamp;

	int dstDataPort;
	int dstControlPort;
	int srcDataPort;
	int srcControlPort;
	int rtpSequenceOffset;
	int rtpTimestampOffset;
	QString dstIp;
	float frameRate;
	int streamedBufferCount;
	QString sdp;
	int bitrateBufSize;
	int bitrate;
	uint totalPacketCount;
	uint totalOctetCount;
	bool sampleNtpRtp;
	bool zeroCopy;
	RawNetworkSocket *rawsock;
} __attribute__ ((deprecated));

#endif // RTPPACKETIZER_H
