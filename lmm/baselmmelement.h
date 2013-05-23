#ifndef BASELMMELEMENT_H
#define BASELMMELEMENT_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QMutex>
#include <QWaitCondition>

#include <lmm/rawbuffer.h>

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
		PAUSED,
	};
	explicit BaseLmmElement(QObject *parent = 0);
	int addBuffer(RawBuffer buffer);
	int addBufferBlocking(RawBuffer buffer);
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
	virtual int setTotalInputBufferSize(int size, int hysterisisSize = 0);

	/* stat information */
	void printStats();
	int getInputBufferCount();
	virtual int getOutputBufferCount();
	int getReceivedBufferCount() { return receivedBufferCount; }
	int getSentBufferCount() { return sentBufferCount; }
	int getFps() { return elementFps; }
	UnitTimeStat * getOutputTimeStat() { return outputTimeStat; }
	int getAvailableDuration();
	void setEnabled(bool val);
	bool isEnabled();
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
	bool enabled;
	QMutex inputLock;
	QMutex outputLock;
	QWaitCondition inputWaiter;
	int totalInputBufferSize;
	int inputHysterisisSize;

	void addNewOutputSemaphore();
	int releaseInputSem(int ch, int count = 1);
	int releaseOutputSem(int ch, int count = 1);
	bool acquireInputSem(int ch) __attribute__((warn_unused_result));
	bool acquireOutputSem(int ch) __attribute__((warn_unused_result));
	virtual void updateOutputTimeStats();
	virtual void calculateFps();
	RunningState getState();
	int setState(RunningState s);
private:
	QMap<QString, QVariant> parameters;

	int elementFps;
	int fpsBufferCount;
	QTime *fpsTiming;
	UnitTimeStat *outputTimeStat;
	qint64 lastOutputTimeStat;
	RunningState state;

	QList<QSemaphore *> bufsem;
	QList<QSemaphore *> inbufsem;

	int checkSizeLimits();
	void checkAndWakeInputWaiters();
};

#endif // BASELMMELEMENT_H
