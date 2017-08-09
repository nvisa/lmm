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
	RawBuffer take(bool keepRef = true);
	int give(RawBuffer buf);
	int give(int index);

	void finalize();
protected:
	QList<RawBuffer> buffersFree;
	QHash<int, RawBuffer> buffersUsed;
	QMutex mutex;
	QWaitCondition wc;
	bool finalized;
};

#endif // LMMBUFFERPOOL_H
