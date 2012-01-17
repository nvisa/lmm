#ifndef BASELMMELEMENT_H
#define BASELMMELEMENT_H

#include <QObject>
#include <QList>

class RawBuffer;
class QTime;

class BaseLmmElement : public QObject
{
	Q_OBJECT
public:
	explicit BaseLmmElement(QObject *parent = 0);
	int addBuffer(RawBuffer *buffer);
	RawBuffer * nextBuffer();
	void setStreamTime(QTime *t) { streamTime = t; }
	void setStreamDuration(qint64 duration) { streamDuration = duration; }
	void printStats();
	virtual int start() { return 0; }
	virtual int stop() { return 0; }
signals:
	
public slots:
protected:
	QList<RawBuffer *> inputBuffers;
	QList<RawBuffer *> outputBuffers;
	QTime *streamTime;
	qint64 streamDuration;
	int inputBufferCount;
	int outputBufferCount;
};

#endif // BASELMMELEMENT_H
