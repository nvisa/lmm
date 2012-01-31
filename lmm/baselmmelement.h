#ifndef BASELMMELEMENT_H
#define BASELMMELEMENT_H

#include <QObject>
#include <QList>

class RawBuffer;
class StreamTime;
class CircularBuffer;

class BaseLmmElement : public QObject
{
	Q_OBJECT
public:
	explicit BaseLmmElement(QObject *parent = 0);
	int addBuffer(RawBuffer *buffer);
	virtual RawBuffer * nextBuffer();
	void setStreamTime(StreamTime *t) { streamTime = t; }
	void setStreamDuration(qint64 duration) { streamDuration = duration; }
	virtual CircularBuffer * getCircularBuffer() { return NULL; }
	virtual qint64 getLatency() { return 0; }
	virtual int start();
	virtual int stop();
	virtual int flush();

	/* stat information */
	void printStats();
	int getInputBufferCount() { return inputBuffers.size(); }
	int getOutputBufferCount() { return outputBuffers.size(); }
	int getReceivedBufferCount() { return receivedBufferCount; }
	int getSentBufferCount() { return sentBufferCount; }
signals:
	
public slots:
protected:
	QList<RawBuffer *> inputBuffers;
	QList<RawBuffer *> outputBuffers;
	StreamTime *streamTime;
	qint64 streamDuration;
	int receivedBufferCount;
	int sentBufferCount;
};

#endif // BASELMMELEMENT_H
