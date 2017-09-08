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
class RateReducto;
class RawBufferParameters;
class QElapsedTimer;
class ElementIOQueue;

class BaseLmmElement : public QObject
{
	Q_OBJECT
public:
	typedef void (*eventHook)(BaseLmmElement *, const RawBuffer &, int, void *);
	enum Events {
		EV_PROCESS,
	};
	enum RunningState {
		INIT,
		STARTED,
		STOPPED,
		PAUSED,
	};
	explicit BaseLmmElement(QObject *parent = 0);
	virtual int addBuffer(int ch, const RawBuffer &buffer);
	RawBuffer nextBufferBlocking(int ch);
	virtual int process(int ch = 0);
	virtual int process(int ch, const RawBuffer &buf);
	virtual int processBlocking(int ch = 0);
	void setStreamTime(StreamTime *t) { streamTime = t; }
	StreamTime * getStreamTime();
	void setStreamDuration(qint64 duration) { streamDuration = duration; }
	virtual CircularBuffer * getCircularBuffer() { return NULL; }
	virtual qint64 getLatency() { return 0; }
	virtual int start();
	virtual int stop();
	virtual int flush();
	virtual int prepareStop();
	virtual int setParameter(QString param, QVariant value);
	virtual QVariant getParameter(QString param);
	virtual void aboutToDeleteBuffer(const RawBufferParameters *) {}
	virtual void signalReceived(int) {}
	virtual void threadFinished(LmmThread *);
	void setPassThru(bool v) { passThru = v; }
	bool isPassThru() { return passThru; }
	void setEventHook(eventHook hook, void *priv);

	UnitTimeStat * getProcessTimeStat() { return processTimeStat; }
	void setEnabled(bool val);
	bool isEnabled();
	bool isRunning();
	virtual QList<QVariant> extraDebugInfo();

	void addOutputQueue(ElementIOQueue *q);
	void addInputQueue(ElementIOQueue *q);
	void setInputQueue(int ch, ElementIOQueue *q);
	void setOutputQueue(int ch, ElementIOQueue *q);
	ElementIOQueue * getOutputQueue(int ch);
	ElementIOQueue * getInputQueue(int ch);
	ElementIOQueue * createIOQueue();
	int getInputQueueCount();
	int getOutputQueueCount();
	virtual qint64 getTotalMemoryUsage();

	/* for settings framework */
	virtual int setSetting(const QString &setting, const QVariant &value);
	virtual QVariant getSetting(const QString &setting);
signals:
	void needFlushing();
	void newBufferAvailable();
public slots:
protected:
	int prependInputBuffer(int ch, const RawBuffer &buf);
	virtual int processBuffer(const RawBuffer &buf) = 0;
	virtual int processBuffer(int ch, const RawBuffer &buf);
	RunningState getState();
	int setState(RunningState s);
	virtual int newOutputBuffer(const RawBuffer &buf);
	virtual int newOutputBuffer(int ch, const RawBuffer &buf);
	virtual int newOutputBuffer(int ch, QList<RawBuffer> list);
	void notifyEvent(Events ev, const RawBuffer &buf);

	StreamTime *streamTime;
	qint64 streamDuration;
	UnitTimeStat *processTimeStat;
	bool passThru;
	bool eofSent;
private:
	QList<int> inBufSize;
	bool enabled;

	QMap<QString, QVariant> parameters;

	RunningState state;

	QList<ElementIOQueue *> inq;
	QMutex inql;
	QList<ElementIOQueue *> outq;
	QMutex outql;

	QMutex evLock;
	eventHook evHook;
	void *evPriv;
};

class ElementIOQueue {
public:
	typedef void (*eventHook)(ElementIOQueue *, const RawBuffer &, int, BaseLmmElement *, void *);
	enum Events {
		EV_ADD,
		EV_GET,
	};
	enum RateLimit {
		LIMIT_NONE,
		LIMIT_INTERVAL,
		LIMIT_BUFFER_COUNT,
		LIMIT_TOTAL_SIZE,
	};

	ElementIOQueue();

	int waitBuffers(int lessThan);
	int addBuffer(const RawBuffer &buffer, BaseLmmElement *src);
	int addBuffer(const QList<RawBuffer> &list, BaseLmmElement *src);
	int prependBuffer(const RawBuffer &buffer);
	RawBuffer getBuffer(BaseLmmElement *src);
	RawBuffer getBufferNW(BaseLmmElement *src);
	int getSemCount() const;
	int getBufferCount();
	void clear();
	void start();
	void stop();
	int setSizeLimit(int size, int hsize);
	float getFps() const { return fps; }
	int getBitrate() const { return bitrate; }
	int getReceivedCount() const { return receivedCount; }
	int getSentCount() const { return sentCount; }
	void setEventHook(eventHook hook, void *priv);
	int getTotalSize() { return bufSize; }
	int setRateReduction(float inFps, float outFps);
	int setRateLimitInterval(qint64 interval);
	int setRateLimitBufferCount(int count);
	int setRateLimitTotalSize(int size);
	RateLimit getRateLimit() { return rlimit; }
	qint64 getElapsedSinceLastAdd();

protected:
	void rateLimit(const RawBuffer &buffer);
	bool acquireSem() __attribute__((warn_unused_result));
	int checkSizeLimits();
	void calculateFps();
	void notifyEvent(Events ev, const RawBuffer &buf, BaseLmmElement *src);

	QList<RawBuffer> queue;
	int bufSize;
	QMutex lock;
	int receivedCount;
	int sentCount;
	QSemaphore * bufSem;
	int bufSizeLimit;
	BaseLmmElement::RunningState state;
	int hysterisisSize;
	int outputWakeThreshold;
	QWaitCondition * outWc;
	RateLimit rlimit;
	qint64 limitInterval;
	int limitBufferCount;
	int limitTotalSize;
	QElapsedTimer *rlimitTimer;

	int _bitrate;
	int bitrate;
	int fpsBufferCount;
	QElapsedTimer * fpsTiming;
	float fps;
	RateReducto *rc;

private:
	QMutex evLock;
	eventHook evHook;
	void *evPriv;
};

#endif // BASELMMELEMENT_H
