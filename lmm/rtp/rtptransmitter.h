#ifndef RTPTRANSMITTER_H
#define RTPTRANSMITTER_H

#include <lmm/lmmcommon.h>
#include <lmm/baselmmelement.h>
#include <lmm/tools/rawnetworksocket.h>
#include <lmm/interfaces/streamcontrolelementinterface.h>

#include <QHostAddress>
#include <QElapsedTimer>

#include <sys/socket.h>

class MyTime;
class QTimer;
class QUdpSocket;
class TokenBucket;

class RtpChannel : public QObject
{
	Q_OBJECT
public:
	RtpChannel(int psize, const QHostAddress &myIpAddr);
	virtual ~RtpChannel();
	QString getSdp(Lmm::CodecType codec);

	typedef int (*transportHook)(const char *data, int size, RtpChannel *, void *priv);

	int rtcpTimeoutValue;
	int seqFirst;
	int seq;
	uint ssrc;
	uint baseTs;
	int dstDataPort;
	int dstControlPort;
	int srcDataPort;
	int srcControlPort;
	int rtpSequenceOffset;
	int rtpTimestampOffset;
	MyTime *rtcpTime;
	MyTime *rrTime;
	QString dstIp;
	socklen_t ttl;
	int bufferCount;
	TokenBucket *tb;
	bool useSR;
	uint lastRtpTs;
	transportHook trHook;
	void *trHookPriv;
	int interleaved;

signals:
	void goodbyeRecved();
	void sessionTimedOut();
protected slots:
	void timeout();
	void readPendingRtcpDatagrams();

protected:
	friend class RtpTransmitter;
	bool initZeroCopy();
	int play();
	int teardown();
	int setup(const QString &target, int dport, int dcport, int sport, int scport, uint ssrc);
	int sendNalUnit(const uchar *buf, int size, qint64 ts);
	int sendPcmData(const uchar *buf, int size, qint64 ts);
	int sendJpegData(const uchar *buf, int size, qint64 ts);
	void sendRtpData(uchar *buf, int size, int last, void *sbuf, qint64 tsRef);
	void sendSR(quint64 bufferTs);
	RawNetworkSocket::SockBuffer * getSBuf();
	uchar * getRtpBuf(RawNetworkSocket::SockBuffer *sbuf);
	virtual void writeSockData(const char *buf, int size);

	QHostAddress myIpAddr;
	int maxPayloadSize;
	bool zeroCopy;
	QUdpSocket *sock;
	QUdpSocket *sock2;
	RawNetworkSocket *rawsock;
	QMutex sockLock;
	uchar *tempRtpBuf;
	uint totalPacketCount;
	uint totalOctetCount;
	int state; /* 0: init/stop, 1:setup, 2:play */
	QTimer *timer;
	int payloadType;
	uint rtcpTs;
	qint64 ntpEpoch;
};

class RtpTransmitter : public BaseLmmElement, public StreamControlElementInterface
{
	Q_OBJECT
public:
	explicit RtpTransmitter(QObject *parent = 0, Lmm::CodecType codec = Lmm::CODEC_H264);

	virtual RtpChannel * addChannel();
	int getChannelCount();
	RtpChannel * getChannel(int ch);
	void removeChannel(RtpChannel *ch);
	Lmm::CodecType getCodec();
	virtual int start();
	virtual int stop();
	virtual int setupChannel(RtpChannel *ch, const QString &target, int dport, int dcport, int sport, int scport, uint ssrc);
	int playChannel(RtpChannel *ch);
	int teardownChannel(RtpChannel *ch);
	void setMulticastTTL(socklen_t ttl);
	void setMaximumPayloadSize(int value);
	bool isActive();
	int setTrafficShaping(bool enabled, int average, int burst, int duration = 50);
	void setRtcp(bool enabled);
	void setH264SEIInsertion(bool v) { insertH264Sei = v; }
	void setRtcpTimeoutValue(int v) { rtcpTimeoutValue = v; }
	int getRtcpTimeoutValue() { return rtcpTimeoutValue; }
	void useIncomingTimestamp(bool v) { useIncomingTs = v; }
	bool isUsingIncomingTimestamp() { return useIncomingTs; }
	void forwardRtpTs(bool v) { forwardIncomingTimestamp = v; }
	void setFrameRate(float fps) { frameRate = fps; }
	void setUseAbsoluteTimestamp(bool v) { useAbsoluteTimestamp = v; }
protected:
	int processBuffer(const RawBuffer &buf);
	quint64 packetTimestamp();
	void packetizeAndSend(const RawBuffer &buf);
	void sendPcmData(const RawBuffer &buf);
	void sendMetaData(const RawBuffer &buf);
	void sendJpegData(const RawBuffer &buf);

	/* channel operations */
	void channelsSendNal(const uchar *buf, int size, qint64 ts);
	void channelsSendRtp(uchar *buf, int size, int last, void *sbuf, qint64 tsRef);
	void channelsCheckRtcp(quint64 ts);
	virtual RtpChannel * createChannel();

	QHostAddress myIpAddr;
	QList<RtpChannel *> channels;
	int streamedBufferCount;
	bool useAbsoluteTimestamp;
	float frameRate;
	bool useStapA;
	int maxPayloadSize;
	QMutex streamLock;
	socklen_t ttl;
	bool waitIdrFrame;
	Lmm::CodecType mediaCodec;
	qint64 lastBufferTime;
	TokenBucket *tb;
	bool insertH264Sei;
	int rtcpTimeoutValue;
	bool useIncomingTs;
	qint64 incomingRtpTimestamp;
	bool forwardIncomingTimestamp;

	int bufferCount;
	bool rtcpEnabled;

	struct TrafficShapingInfo {
		bool enabled;
		int avgBitsPerSec;
		int burstBitsPerSec;
		int controlDuration;
	};
	TrafficShapingInfo tsinfo;
};

#endif // RTPTRANSMITTER_H
