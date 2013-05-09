#include "buffersyncer.h"

#include <lmm/streamtime.h>

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

int BufferSyncer::addBuffer(BaseLmmElement *target, RawBuffer buffer)
{
	if (buffer.size() == 0)
		return -EINVAL;

	qint64 pts = buffer.getPts();
	qint64 delay = streamTime->ptsToTimeDiff(pts);
	BufferSyncThread *th = new BufferSyncThread(target, buffer, delay);
	th->start(QThread::LowestPriority);
	inputLock.lock();
	threads << th;
	receivedBufferCount++;
	inputLock.unlock();

	return 0;
}
