#ifndef RTPTRANSMITTER_H
#define RTPTRANSMITTER_H

#include <lmm/lmmcommon.h>
#include <lmm/baselmmelement.h>
#include <lmm/tools/rawnetworksocket.h>
#include <lmm/interfaces/streamcontrolelementinterface.h>

#include <QHostAddress>
#include <QElapsedTimer>

class MyTime;
class QTimer;
class QUdpSocket;

class RtpChannel : public QObject
{
	Q_OBJECT
public:
	RtpChannel(int psize, const QHostAddress &myIpAddr);
	~RtpChannel();
	QString getSdp(Lmm::CodecType codec);

	int seq;
	uint ssrc;
	uint baseTs;
	int dstDataPort;
	int dstControlPort;
	int srcDataPort;
	int srcControlPort;
	int rtpSequenceOffset;
	int rtpTimestampOffset;
	QPair<qint64, uint> ntpRtpPair;
	MyTime *rtcpTime;
	MyTime *rrTime;
	QString dstIp;
	socklen_t ttl;
	int bufferCount;

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
	void sendRtpData(uchar *buf, int size, int last, void *sbuf, qint64 tsRef);
	void sendSR();
	RawNetworkSocket::SockBuffer * getSBuf();
	uchar * getRtpBuf(RawNetworkSocket::SockBuffer *sbuf);

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
};

class RtpTransmitter : public BaseLmmElement, public StreamControlElementInterface
{
	Q_OBJECT
public:
	explicit RtpTransmitter(QObject *parent = 0, Lmm::CodecType codec = Lmm::CODEC_H264);

	RtpChannel * addChannel();
	int getChannelCount();
	RtpChannel * getChannel(int ch);
	void removeChannel(RtpChannel *ch);
	Lmm::CodecType getCodec();
	virtual int start();
	virtual int stop();
	int setupChannel(RtpChannel *ch, const QString &target, int dport, int dcport, int sport, int scport, uint ssrc);
	int playChannel(RtpChannel *ch);
	int teardownChannel(RtpChannel *ch);
	void setMulticastTTL(socklen_t ttl);
	void setMaximumPayloadSize(int value);
	bool isActive();
public slots:
	void sampleNtpTime();
protected:
	int processBuffer(const RawBuffer &buf);
	quint64 packetTimestamp();
	void packetizeAndSend(const RawBuffer &buf);
	void sendPcmData(const RawBuffer &buf);
	void sendMetaData(const RawBuffer &buf);

	/* channel operations */
	void channelsSetTimestamp(qint64 current, qint64 packet);
	void channelsSendNal(const uchar *buf, int size, qint64 ts);
	void channelsSendRtp(uchar *buf, int size, int last, void *sbuf, qint64 tsRef);
	void channelsCheckRtcp();

	QHostAddress myIpAddr;
	QList<RtpChannel *> channels;
	int streamedBufferCount;
	bool sampleNtpRtp;
	int bitrateBufSize;
	int bitrate;
	bool useAbsoluteTimestamp;
	float frameRate;
	bool useStapA;
	int maxPayloadSize;
	QMutex streamLock;
	socklen_t ttl;
	bool waitIdrFrame;
	Lmm::CodecType mediaCodec;
	qint64 lastBufferTime;
	QPair<qint64, uint> ntpRtpPair;
};

#endif // RTPTRANSMITTER_H
