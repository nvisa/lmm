#ifndef BASELMMOUTPUT_H
#define BASELMMOUTPUT_H

#include <lmm/baselmmelement.h>

class BaseLmmOutput : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmOutput(QObject *parent = 0);
	virtual qint64 getAvailableBufferTime() { return 0; }
	void setOutputDelay(int val) { outputDelay = val; }
	int getOutputDelay() { return outputDelay; }
	void syncOnClock(bool val) { doSync = val; }
	virtual int start();
	virtual int stop();
	virtual qint64 getLatency();
	virtual int getLoopLatency();
	qint64 getLastOutputPts() { return lastOutputPts; }
signals:
	
public slots:
protected:
	virtual int processBuffer(const RawBuffer &buf);
	virtual int outputBuffer(RawBuffer buf);
	qint64 outputLatency;
	int outputDelay;
	bool doSync;
	qint64 lastOutputPts;
private:
};

#endif // BASELMMOUTPUT_H
