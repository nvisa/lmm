#ifndef LMMBUFFERPOOL_H
#define LMMBUFFERPOOL_H

#include <QList>
#include <QMutex>
#include <QObject>
#include <QWaitCondition>

#include <lmm/rawbuffer.h>

class LmmBufferPool : public QObject
{
	Q_OBJECT
public:
	explicit LmmBufferPool(QObject *parent = 0);
	explicit LmmBufferPool(int count, int size, QObject *parent = 0);

	int addBuffer(RawBuffer buf);
	int usedBufferCount();
	int freeBufferCount();
	RawBuffer take();
	int give(RawBuffer buf);
protected:
	QList<RawBuffer> buffersFree;
	QList<RawBuffer> buffersUsed;
	QMutex mutex;
	QWaitCondition wc;
};

#endif // LMMBUFFERPOOL_H
