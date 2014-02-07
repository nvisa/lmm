#ifndef RTPPACKETIZER_H
#define RTPPACKETIZER_H

#include <lmm/lmmcommon.h>
#include <lmm/baselmmelement.h>

#include <QTime>
#include <QHostAddress>

class QUdpSocket;

class RtpPacketizer : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit RtpPacketizer(QObject *parent = 0);
	Lmm::CodecType codecType();
	virtual int start();
	virtual int stop();

	void sampleNtpTime();
	void setPassThru(bool v) { passThru = v; }
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

	bool isPacketized() { return packetized; }
	void setPacketized(bool v) { packetized = v; }
signals:
	void sdpReady(QString sdp);
	void newReceiverReport(int);
protected slots:
	void readPendingRtcpDatagrams();
protected:
	virtual void calculateFps(const RawBuffer buf);
	virtual int processBuffer(RawBuffer buf);
	virtual quint64 packetTimestamp(int stream);
	int sendNalUnit(const uchar *buf, int size);
	void sendRtpData(uchar *buf, int size, int last);
	void createSdp();
	void sendSR();

	int maxPayloadSize;
	int seq;
	uint ssrc;
	uint baseTs;
	QUdpSocket *sock;
	QUdpSocket *sock2; //rtcp socket
	QMutex streamLock;
	bool packetized;
	QPair<qint64, uint> ntpRtpPair;
	QTime rtcpTime;
	QHostAddress myIpAddr;

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
	bool passThru;
	int bitrateBufSize;
	int bitrate;
	uint totalPacketCount;
	uint totalOctetCount;
	bool sampleNtpRtp;
};

#endif // RTPPACKETIZER_H
