#ifndef RTSPSERVER_H
#define RTSPSERVER_H

#include "baselmmelement.h"

class Live555Thread;

class RtspServer : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit RtspServer(QObject *parent = 0);
	int start();
	int stop();
	int addBuffer(RawBuffer buffer);
signals:
	
public slots:
private:
	Live555Thread *liveThread;
};

#endif // RTSPSERVER_H
