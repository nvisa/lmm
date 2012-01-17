#ifndef BASELMMOUTPUT_H
#define BASELMMOUTPUT_H

#include "baselmmelement.h"

class BaseLmmOutput : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmOutput(QObject *parent = 0);
	
signals:
	
public slots:
protected:
	int checkBufferTimeStamp(RawBuffer *, int jitter = 1);
	
};

#endif // BASELMMOUTPUT_H
