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
class LmmThread;
class RawBufferParameters;

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
	int addBuffer(int ch, RawBuffer buffer);
	int addBufferBlocking(int ch, RawBuffer buffer);
	int addBuffersBlocking(int ch, const QList<RawBuffer> list);
	RawBuffer nextBuffer(int ch);
	RawBuffer nextBufferBlocking(int ch);
	QList<RawBuffer> nextBuffers(int ch);
	QList<RawBuffer> nextBuffersBlocking(int ch);
	virtual int process(int ch = 0);
	virtual int process(int ch, RawBuffer buf);
	virtual int processBlocking(int ch = 0);
	virtual int processBlocking(int ch, RawBuffer buf);
	void setStreamTime(StreamTime *t) { streamTime = t; }
	void setStreamDuration(qint64 duration) { streamDuration = duration; }
	virtual CircularBuffer * getCircularBuffer() { return NULL; }
	virtual qint64 getLatency() { return 0; }
	virtual int start();
	virtual int stop();
	virtual int flush();
	int sendEOF();
	virtual int setParameter(QString param, QVariant value);
	virtual QVariant getParameter(QString param);
	virtual void aboutToDeleteBuffer(const RawBufferParameters *) {}
	virtual void signalReceived(int) {}
	virtual int setTotalInputBufferSize(int size, int hysterisisSize = 0);
	int waitOutputBuffers(int ch, int lessThan);
	virtual void threadFinished(LmmThread *) {}

	/* stat information */
	void printStats();
	int getInputBufferCount();
	virtual int getOutputBufferCount();
	int getReceivedBufferCount() { return receivedBufferCount; }
	int getSentBufferCount() { return sentBufferCount; }
	int getFps() { return elementFps; }
	UnitTimeStat * getOutputTimeStat() { return outputTimeStat; }
	UnitTimeStat * getProcessTimeStat() { return processTimeStat; }
	int getAvailableDuration();
	void setEnabled(bool val);
	bool isEnabled();
	bool isRunning();
	int getInputSemCount(int ch);
	int getOutputSemCount(int ch);
	virtual QList<QVariant> extraDebugInfo();
signals:
	void needFlushing();
	void newBufferAvailable();
public slots:
protected:
	void addNewInputChannel();
	void addNewOutputChannel();
	int releaseInputSem(int ch, int count = 1);
	int releaseOutputSem(int ch, int count = 1);
	bool acquireInputSem(int ch) __attribute__((warn_unused_result));
	bool acquireOutputSem(int ch) __attribute__((warn_unused_result));
	RawBuffer takeInputBuffer(int ch);
	int appendInputBuffer(int ch, RawBuffer buf);
	int prependInputBuffer(int ch, RawBuffer buf);
	virtual int processBuffer(RawBuffer buf) = 0;
	virtual int processBuffer(int ch, RawBuffer buf);
	virtual void updateOutputTimeStats();
	virtual void calculateFps(const RawBuffer buf);
	RunningState getState();
	int setState(RunningState s);
	virtual int checkSizeLimits();
	virtual void checkAndWakeInputWaiters();
	virtual int newOutputBuffer(int ch, RawBuffer buf);
	virtual int newOutputBuffer(int ch, QList<RawBuffer> list);

	StreamTime *streamTime;
	qint64 streamDuration;
	int receivedBufferCount;
	int sentBufferCount;
	UnitTimeStat *processTimeStat;

	int elementFps;
	int fpsBufferCount;
	QTime *fpsTiming;
	bool eofSent;
private:
	QList< QList<RawBuffer> > inBufQueue;
	QList< QList<RawBuffer> > outBufQueue;
	QList<int> inBufSize;
	bool enabled;
	QMutex inputLock;
	QMutex outputLock;
	QWaitCondition inputWaiter;
	QList<QWaitCondition *> outWc;
	int totalInputBufferSize;
	int inputHysterisisSize;
	int outputWakeThreshold;

	QMap<QString, QVariant> parameters;

	UnitTimeStat *outputTimeStat;
	qint64 lastOutputTimeStat;
	RunningState state;

	QList<QSemaphore *> bufsem;
	QList<QSemaphore *> inbufsem;

};

#endif // BASELMMELEMENT_H
