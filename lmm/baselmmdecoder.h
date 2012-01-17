#ifndef BASELMMDECODER_H
#define BASELMMDECODER_H

#include "baselmmelement.h"

#include <QList>

struct decodeTimeStamp {
	int duration;
	int validDuration;
	int usedDuration;
	qint64 pts;
};

class RawBuffer;

class BaseLmmDecoder : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmDecoder(QObject *parent = 0);
	
signals:
	
public slots:
protected:
	QList<decodeTimeStamp *> inTimeStamps;
	decodeTimeStamp *timestamp;

	void handleInputTimeStamps(RawBuffer *buf);
	void setOutputTimeStamp(RawBuffer *buf, int minDuration = 2000);
};

#endif // BASELMMDECODER_H
