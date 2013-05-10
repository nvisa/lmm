#include "buffersyncer.h"

#include <lmm/debug.h>
#include <lmm/streamtime.h>
#include <lmm/baselmmoutput.h>

#include <QThread>
#include <QSemaphore>

#include <errno.h>

class BufferSyncThread : public QThread
{
public:
	BufferSyncThread(BaseLmmElement *element, RawBuffer buf, qint64 delay)
	{
		buffer = buf;
		sleepTime = delay;
		el = element;
		exit = false;
	}

	BufferSyncThread()
	{
		exit = false;
	}

	void run()
	{
		while (!exit) {
			waitsem.release();
			opsem.acquire();
			usleep(sleepTime);
			el->addBuffer(buffer);
			/* release reference to buffer */
			buffer = RawBuffer();
		}
	}

	bool isFree()
	{
		return waitsem.available();
	}

	void addBuffer(BaseLmmElement *element, RawBuffer buf, qint64 delay)
	{
		buffer = buf;
		sleepTime = delay;
		el = element;
		waitsem.acquire();
		opsem.release();
	}

	BaseLmmElement *el;
	RawBuffer buffer;
	qint64 sleepTime;
	QSemaphore opsem;
	QSemaphore waitsem;
	bool exit;
};

BufferSyncer::BufferSyncer(int threadCount, QObject *parent) :
	BaseLmmElement(parent),
	sync(true)
{
	for (int i = 0; i < threadCount; i++) {
		BufferSyncThread *th = new BufferSyncThread();
		th->start(QThread::LowestPriority);
		threads << th;
	}
}

int BufferSyncer::addBuffer(BaseLmmOutput *target, RawBuffer buffer)
{
	if (buffer.size() == 0)
		return -EINVAL;

	qint64 pts = buffer.getPts();
	qint64 delay = streamTime->ptsToTimeDiff(pts) - target->getAvailableBufferTime();
	if (sync && delay > 0) {
		BufferSyncThread *th = findEmptyThread();
		if (th)
			th->addBuffer(target, buffer, delay);
		else
			mDebug("cannot find an empty thread, skipping");
	} else {
		target->addBuffer(buffer);
		receivedBufferCount++;
	}

	return 0;
}

BufferSyncThread * BufferSyncer::findEmptyThread()
{
	foreach(BufferSyncThread *th, threads) {
		if (th->isFree())
			return th;
	}
	return NULL;
}
