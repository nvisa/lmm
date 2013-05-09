#ifndef LMMBUFFERPOOL_H
#define LMMBUFFERPOOL_H

#include <QList>
#include <QMutex>
#include <QObject>

#include <lmm/rawbuffer.h>

class LmmBufferPool : public QObject
{
	Q_OBJECT
public:
	explicit LmmBufferPool(QObject *parent = 0);
	explicit LmmBufferPool(int count, QObject *parent = 0);

	void addBuffer(RawBuffer buf);
	int totalBufferCount();
	int freeBufferCount();
	RawBuffer take();
	int give(RawBuffer buf);
protected:
	QList<RawBuffer> buffersFree;
	QList<RawBuffer> buffersUsed;
	QMutex mutex;
};

#endif // LMMBUFFERPOOL_H
