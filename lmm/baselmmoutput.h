#ifndef BASELMMOUTPUT_H
#define BASELMMOUTPUT_H

#include "baselmmelement.h"

class BaseLmmOutput : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmOutput(QObject *parent = 0);
	virtual qint64 getLatency() { return 0; }
	virtual qint64 getAvailableBufferTime() { return 0; }
	void setOutputDelay(int val) { outputDelay = val; }
	int getOutputDelay() { return outputDelay; }
	void syncOnClock(bool val) { doSync = val; }
	virtual int output() = 0;
signals:
	
public slots:
protected:
	int checkBufferTimeStamp(RawBuffer *, int jitter = 1);

	qint64 outputLatency;
private:
	int outputDelay;
	bool doSync;

	qint64 last_rpts;
	qint64 last_time;
};

#endif // BASELMMOUTPUT_H
