#include "bufferqueue.h"

BufferQueue::BufferQueue(QObject *parent) :
	BaseLmmElement(parent)
{
	queueLen = 1;
}

RawBuffer BufferQueue::getBuffer(int ind)
{
	block.lock();
	RawBuffer buf;
	if (buffers.size() > ind)
		buf = buffers[ind];
	block.unlock();
	return buf;
}

RawBuffer BufferQueue::findBuffer(qint64 ts)
{
	int f = -1;
	int min = INT_MAX;
	block.lock();
	for (int i = buffers.size() - 1; i >= 0; i--) {
		int diff = qAbs(ts - buffers[i].constPars()->captureTime);
		if (diff < min) {
			min = diff;
			f = i;
		}
	}
	RawBuffer buf;
	if (f >= 0)
		buf = buffers[f];
	block.unlock();
	return buf;
}

RawBuffer BufferQueue::getLast()
{
	block.lock();
	RawBuffer buf;
	if (buffers.size())
		buf = buffers.last();
	block.unlock();
	return buf;
}

void BufferQueue::setQueueSize(int length)
{
	queueLen = length;
}

int BufferQueue::getBufferCount()
{
	int cnt = 0;
	block.lock();
	cnt = buffers.size();
	block.unlock();
	return cnt;
}

int BufferQueue::processBuffer(const RawBuffer &buf)
{
	block.lock();
	buffers << buf;
	int size = buffers.size();
	if (size >= queueLen) {
		const RawBuffer outbuf = buffers.takeFirst();
		block.unlock();
		return newOutputBuffer(0, outbuf);
	}
	block.unlock();
	return 0;
}
