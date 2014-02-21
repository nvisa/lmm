#ifndef BASELMMDECODER_H
#define BASELMMDECODER_H

#include <lmm/baselmmelement.h>

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
	int start();
	int stop();
	virtual int flush();
signals:
	
public slots:
protected:
	QList<decodeTimeStamp *> inTimeStamps;
	decodeTimeStamp *timestamp;

	void handleInputTimeStamps(RawBuffer *buf);
	void setOutputTimeStamp(RawBuffer *buf, int minDuration = 2000);
	virtual int startDecoding() { return 0; }
	virtual int stopDecoding() { return 0; }
	virtual int processBuffer(const RawBuffer &buf);
	virtual int decode(RawBuffer) = 0;
};

#endif // BASELMMDECODER_H
