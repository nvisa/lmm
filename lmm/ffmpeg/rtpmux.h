#ifndef RTPMUX_H
#define RTPMUX_H

#include <lmm/ffmpeg/baselmmmux.h>
#include <lmm/lmmcommon.h>

class RtpMux : public BaseLmmMux
{
	Q_OBJECT
public:
	explicit RtpMux(QObject *parent = 0);
	virtual QString mimeType();

	int start();
	int stop();
	int sendNext();
	int sendNextBlocking();

	virtual Lmm::CodecType codecType() = 0;
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
	int getLoopLatency() { return loopLatency; }
	QString getSdp();
protected:
	virtual int initMuxer();
signals:
	void sdpReady(QString sdp);
private:
	int dstDataPort;
	int dstControlPort;
	int srcDataPort;
	int srcControlPort;
	int rtpSequenceOffset;
	int rtpTimestampOffset;
	int loopLatency;
	QString dstIp;
};

#endif // RTPMUX_H
