#ifndef BASELMMOUTPUT_H
#define BASELMMOUTPUT_H

#include "baselmmelement.h"

class BaseLmmOutput : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmOutput(QObject *parent = 0);
	virtual qint64 getLatency() { return 0; }
	void setOutputDelay(int val) { outputDelay = val; }
	int getOutputDelay() { return outputDelay; }
	virtual int output() = 0;
signals:
	
public slots:
protected:
	int checkBufferTimeStamp(RawBuffer *, int jitter = 1);
private:
	int outputDelay;
};

#endif // BASELMMOUTPUT_H
