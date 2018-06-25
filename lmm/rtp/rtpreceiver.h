#ifndef RTPRECEIVER_H
#define RTPRECEIVER_H

#include <lmm/baselmmelement.h>

#include <QObject>
#include <QByteArray>
#include <QHostAddress>
#include <QElapsedTimer>

class QFile;
class QUdpSocket;

class RtpReceiver : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit RtpReceiver(QObject *parent = 0);

	struct RtpStats {
		RtpStats()
		{
			packetCount = 0;
			packetMissing = 0;
			headerError = 0;
			payloadErr = 0;
			rtcpVersionError = 0;
		}

		int packetCount;
		int packetMissing;
		int headerError;
		int payloadErr;
		int rtcpVersionError;

		qint64 rtcpEpoch;
		qint64 rtcpMyEpoch;
		uint rtcpTs;
	};
	struct SeiStats {
		bool enabled;
		int cpuload;
		int freemem;
		int uptime;
		int pid;
		int bufferUsage;
		int bufferCount;
		int sessionCount;
		QByteArray meta;
	};

	int getSourceDataPort() { return srcDataPort; }
	int getSourceControlPort() { return srcControlPort; }
	int getDestinationDataPort() { return dstDataPort; }
	int getDestinationControlPort() { return dstControlPort; }
	void setDestinationDataPort(int port) { dstDataPort = port; }
	void setDestinationControlPort(int port) { dstControlPort = port; }
	void setSourceDataPort(int port) { srcDataPort = port; }
	void setSourceControlPort(int port) { srcControlPort = port; }
	void setSourceAddress(const QHostAddress &addr) { sourceAddr = addr; }
	RtpStats getStatistics();
	SeiStats getLastSeiData();
	void setExpectedFramerate(float fps) { expectedFrameRate = fps; }
	float getExpectedFramerate() { return expectedFrameRate; }
	void enableBitrateSummarization(bool v, int interval = 10);
	void enableUserSEIParsing(bool v);
	void useThreadedReading(bool v);

	int start();
	int stop();

signals:
	void newSummarizationDataReady(uint size, qint64 ts);
protected slots:
	void readPendingRtpDatagrams();
	void readPendingRtcpDatagrams();

protected:
	void sendRR(uint ssrc, const QHostAddress &sender);
	virtual int processRtpData(const QByteArray &ba, const QHostAddress &sender, quint16 senderPort);
	int handleRtpData(const QByteArray &ba);
	void h264FramingError();
	int getRtpClock();
	void parseSeiUserData(const RawBuffer &buf);
	int initSockets();
	int processBlocking(int ch);

	int processh264Payload(const QByteArray &ba, uint ts, int last);
	int processMetaPayload(const QByteArray &ba, uint ts, int last);
	int processJpegPayload(const QByteArray &ba, uint ts, int last);
	virtual int processBuffer(const RawBuffer &buf);

	QUdpSocket *sock;
	QUdpSocket *sock2;
	int dstDataPort;
	int dstControlPort;
	int srcDataPort;
	int srcControlPort;
	QElapsedTimer rtcpTime;
	QHostAddress sourceAddr;
	RtpStats stats;
	QMap<uint, QByteArray> rtpData;
	uint rtpPacketOffset;
	float expectedFrameRate;
	bool threadedRead;

	QMutex sockLock;
	uint seqLast;
	QByteArray currentNal;
	QByteArray annexPrefix;
	int maxPayloadSize;
	QByteArray currentData;
	bool containsMissing;
	int validFrameCount;
	uint firstPacketTs;
	bool framingError;
	SeiStats seistats;
	struct BitrateSummarization {
		BitrateSummarization()
		{
			summarizeBitrateStats = false;
			total = 0;
			interval = 10;
		}

		bool summarizeBitrateStats;
		int total;
		int interval;
		QElapsedTimer slot;
	};
	BitrateSummarization bsum;
};

#endif // RTPRECEIVER_H
