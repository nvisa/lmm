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
class QSemaphore;

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
	virtual RawBuffer nextBuffer(int ch);
	virtual RawBuffer nextBufferBlocking(int ch);
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
	void newBufferAvailable();
public slots:
protected:
	QList<RawBuffer> inputBuffers;
	QList<RawBuffer> outputBuffers;
	StreamTime *streamTime;
	qint64 streamDuration;
	int receivedBufferCount;
	int sentBufferCount;

	QMutex inputLock;
	QMutex outputLock;

	QList<QSemaphore *> bufsem;
	QList<QSemaphore *> inbufsem;

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
