#include "buffersyncer.h"

#include <lmm/streamtime.h>
#include <lmm/baselmmoutput.h>

#include <QThread>

#include <errno.h>

class BufferSyncThread : public QThread
{
public:
	BufferSyncThread(BaseLmmElement *element, RawBuffer buf, qint64 delay)
	{
		buffer = buf;
		sleepTime = delay;
		el = element;
	}

	void run()
	{
		usleep(sleepTime);
		el->addBuffer(buffer);
		/* release reference to buffer */
		buffer = RawBuffer();
	}

	BaseLmmElement *el;
	RawBuffer buffer;
	qint64 sleepTime;
};

BufferSyncer::BufferSyncer(QObject *parent) :
	BaseLmmElement(parent)
{
}

int BufferSyncer::addBuffer(BaseLmmOutput *target, RawBuffer buffer)
{
	if (buffer.size() == 0)
		return -EINVAL;

	/* clear finished threads */
	inputLock.lock();
	QList<BufferSyncThread *> finished;
	foreach(BufferSyncThread *th, threads) {
		if (th->isFinished())
			finished << th;
	}
	foreach (BufferSyncThread *th, finished) {
		threads.removeOne(th);
		th->deleteLater();
	}
	inputLock.unlock();

	qint64 pts = buffer.getPts();
	qint64 delay = streamTime->ptsToTimeDiff(pts) - target->getAvailableBufferTime();
	if (delay > 0) {
		BufferSyncThread *th = new BufferSyncThread(target, buffer, delay);
		th->start(QThread::LowestPriority);
		inputLock.lock();
		threads << th;
		inputLock.unlock();
	} else {
		target->addBuffer(buffer);
		receivedBufferCount++;
	}

	return 0;
}
