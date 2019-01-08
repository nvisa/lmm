#include "bufferqueue.h"
#include "debug.h"

#include <QThread>

#include <unistd.h>

BufferQueue::BufferQueue(QObject *parent) :
	BaseLmmElement(parent)
{
	queueLen = 1;
	totalSize = 0;
	processingThread = NULL;
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

QList<RawBuffer> BufferQueue::findBuffers(qint64 ts, int count)
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
	QList<RawBuffer> bufs;
	if (f < 0) {
		block.unlock();
		return bufs;
	}
	for (int i = f; i < f + count; i++)
		if (i < buffers.size())
			bufs << buffers[i];
	block.unlock();
	return bufs;
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

QList<RawBuffer> BufferQueue::getLast(int count)
{
	QList<RawBuffer> bufs;
	block.lock();
	for (int i = buffers.size() - 1; i > buffers.size() - 1 - count && i > -1; i--)
		bufs << buffers[i];
	block.unlock();
	return bufs;
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
	totalSize += buf.size();
	int size = buffers.size();
	if (size >= queueLen) {
		const RawBuffer outbuf = buffers.takeFirst();
		totalSize -= outbuf.size();
		block.unlock();
		return newOutputBuffer(outbuf);
	}
	block.unlock();
	return 0;
}

int BufferQueue::processBlocking(int ch)
{
	if (!processingThread)
		processingThread = QThread::currentThreadId();

	if (processingThread != QThread::currentThreadId()) {
		sleep(1);
		return 0;
	}

	return BaseLmmElement::processBlocking(ch);
}
