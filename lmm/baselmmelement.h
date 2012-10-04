#ifndef BASELMMELEMENT_H
#define BASELMMELEMENT_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QMutex>

#include "rawbuffer.h"

class StreamTime;
class CircularBuffer;
class UnitTimeStat;

class BaseLmmElement : public QObject
{
	Q_OBJECT
public:
	enum RunningState {
		INIT,
		STARTED,
		STOPPED,
	};
	explicit BaseLmmElement(QObject *parent = 0);
	int addBuffer(RawBuffer buffer);
	virtual RawBuffer nextBuffer();
	void setStreamTime(StreamTime *t) { streamTime = t; }
	void setStreamDuration(qint64 duration) { streamDuration = duration; }
	virtual CircularBuffer * getCircularBuffer() { return NULL; }
	virtual qint64 getLatency() { return 0; }
	virtual int start();
	virtual int stop();
	virtual int flush();
	virtual int setParameter(QString param, QVariant value);
	virtual QVariant getParameter(QString param);
	virtual void aboutDeleteBuffer(const QMap<QString, QVariant> &) {}
	virtual void signalReceived(int) {}
	virtual int setThreaded(bool);
	bool isThreaded() { return threaded; }

	/* stat information */
	void printStats();
	int getInputBufferCount() { return inputBuffers.size(); }
	virtual int getOutputBufferCount() { return outputBuffers.size(); }
	int getReceivedBufferCount() { return receivedBufferCount; }
	int getSentBufferCount() { return sentBufferCount; }
	int getFps() { return elementFps; }
	UnitTimeStat * getOutputTimeStat() { return outputTimeStat; }
	int getAvailableDuration();
signals:
	void needFlushing();
public slots:
protected:
	QList<RawBuffer> inputBuffers;
	QList<RawBuffer> outputBuffers;
	StreamTime *streamTime;
	qint64 streamDuration;
	int receivedBufferCount;
	int sentBufferCount;
	bool threaded;

	QMutex inputLock;
	QMutex outputLock;

	virtual void updateOutputTimeStats();
	virtual void calculateFps();
	RunningState getState();
private:
	QMap<QString, QVariant> parameters;

	int elementFps;
	int fpsBufferCount;
	QTime *fpsTiming;
	UnitTimeStat *outputTimeStat;
	qint64 lastOutputTimeStat;
	RunningState state;
};

#endif // BASELMMELEMENT_H
