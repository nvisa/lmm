#ifndef BASELMMOUTPUT_H
#define BASELMMOUTPUT_H

#include "baselmmelement.h"

class QTime;

class BaseLmmOutput : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmOutput(QObject *parent = 0);
	virtual qint64 getAvailableBufferTime() { return 0; }
	void setOutputDelay(int val) { outputDelay = val; }
	int getOutputDelay() { return outputDelay; }
	void syncOnClock(bool val) { doSync = val; }
	virtual int output();
	virtual qint64 getLatency();
signals:
	
public slots:
protected:
	int checkBufferTimeStamp(RawBuffer *, int jitter = 1);
	virtual int outputBuffer(RawBuffer *buf);
	qint64 outputLatency;
private:
	int outputDelay;
	bool doSync;

	qint64 last_rpts;
	qint64 last_time;
	int fps;
	int fpsBufferCount;
	QTime *fpsTiming;
};

#endif // BASELMMOUTPUT_H
