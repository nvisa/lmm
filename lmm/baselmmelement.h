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
class QElapsedTimer;
class ElementIOQueue;

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
	virtual int addBuffer(int ch, const RawBuffer &buffer);
	RawBuffer nextBufferBlocking(int ch);
	virtual int process(int ch = 0);
	virtual int process(int ch, const RawBuffer &buf);
	virtual int processBlocking(int ch = 0);
	virtual int processBlocking2(int ch, const RawBuffer &buf);
	void setStreamTime(StreamTime *t) { streamTime = t; }
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
	virtual void threadFinished(LmmThread *) {}
	void setPassThru(bool v) { passThru = v; }

	UnitTimeStat * getProcessTimeStat() { return processTimeStat; }
	void setEnabled(bool val);
	bool isEnabled();
	bool isRunning();
	virtual QList<QVariant> extraDebugInfo();

	void addOutputQueue(ElementIOQueue *q);
	void addInputQueue(ElementIOQueue *q);
	ElementIOQueue * getOutputQueue(int ch);
	ElementIOQueue * getInputQueue(int ch);
	ElementIOQueue * createIOQueue();
	int getInputQueueCount();
	int getOutputQueueCount();
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
	virtual int newOutputBuffer(int ch, const RawBuffer &buf);
	virtual int newOutputBuffer(int ch, QList<RawBuffer> list);

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

};

class ElementIOQueue {
public:
	typedef void (*eventHook)(ElementIOQueue *, const RawBuffer &, int, void *);
	enum Events {
		EV_ADD,
		EV_GET,
	};

	ElementIOQueue();

	int waitBuffers(int lessThan);
	int addBuffer(const RawBuffer &buffer);
	int addBuffer(const QList<RawBuffer> &list);
	int prependBuffer(const RawBuffer &buffer);
	RawBuffer getBuffer();
	RawBuffer getBufferNW();
	int getSemCount();
	int getBufferCount();
	void clear();
	void start();
	void stop();
	int setSizeLimit(int size, int hsize);
	int getFps() { return fps; }
	int getReceivedCount() { return receivedCount; }
	int getSentCount() { return sentCount; }
	void setEventHook(eventHook hook, void *priv);

protected:
	bool acquireSem() __attribute__((warn_unused_result));
	int checkSizeLimits();
	void calculateFps();

	QList<RawBuffer> queue;
	int bufSize;
	QMutex lock;
	int totalSize;
	int receivedCount;
	int sentCount;
	QSemaphore * bufSem;
	int bufSizeLimit;
	BaseLmmElement::RunningState state;
	int hysterisisSize;
	int outputWakeThreshold;
	QWaitCondition * outWc;

	int fpsBufferCount;
	QElapsedTimer * fpsTiming;
	int fps;
	eventHook evHook;
	void *evPriv;
};

#endif // BASELMMELEMENT_H
