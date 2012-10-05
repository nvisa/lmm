#ifndef RTPH264MUX_H
#define RTPH264MUX_H

#include "baselmmmux.h"

class RtpH264Mux : public BaseLmmMux
{
	Q_OBJECT
public:
	explicit RtpH264Mux(QObject *parent = 0);
	QString mimeType();


	int start();
	int stop();
	int addBuffer(RawBuffer buffer);

	void setDestinationIpAddress(QString ip) { dstIp = ip; }
	void setRtpSequenceOffset(int value) { rtpSequenceOffset = value; }
	void setRtpTimestampOffset(int value) { rtpTimestampOffset = value; }
	void setDestinationDataPort(int port) { dstDataPort = port; }
	void setDestinationControlPort(int port) { dstControlPort = port; }
	int getSourceDataPort() { return srcDataPort; }
	int getSourceControlPort() { return srcControlPort; }
	int getLoopLatency() { return loopLatency; }
protected:
	int findInputStreamInfo();
	int initMuxer();
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

#endif // RTPH264MUX_H
