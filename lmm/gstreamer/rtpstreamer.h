#ifndef RTPSTREAMER_H
#define RTPSTREAMER_H

#include "abstractgstreamerinterface.h"
#include "baselmmelement.h"
#include "rawbuffer.h"

class QUdpSocket;

class RtpStreamer : public AbstractGstreamerInterface
{
	Q_OBJECT
public:
	explicit RtpStreamer(QObject *parent = 0);
	void sendBuffer(RawBuffer buf);
	void setMediaSource(QString source) { vFileName = source; }
	QString getMediaSource() { return vFileName; }
	int startPlayback();
	int stopPlayback();

	void pushNextBuffer();

	void setRtpSequenceOffset(int value) { rtpSequenceOffset = value; }
	void setRtpTimestampOffset(int value) { rtpTimestampOffset = value; }
	void setDestinationDataPort(int port);
	void setDestinationControlPort(int port) { dstControlPort = port; }
	//void setSourceDataPort(int port) { srcDataPort = port; }
	//void setSourceControlPort(int port) { srcControlPort = port; }
	int getSourceDataPort() { return srcDataPort; }
	int getSourceControlPort() { return srcControlPort; }

	int getWaitingBufferCount() { return inputBuffers.size(); }
signals:
	
public slots:
private:
	bool needData;
	QList<RawBuffer> inputBuffers;
	QList<GstBuffer *> gstBuffers;
	GstElement *appsrc;
	QString vFileName;

	int dstDataPort;
	int dstControlPort;
	int srcDataPort;
	int srcControlPort;
	int rtpSequenceOffset;
	int rtpTimestampOffset;
	QUdpSocket* srcDataSock;

	int createElementsList();
	void pipelineCreated();
};

#endif // RTPSTREAMER_H
