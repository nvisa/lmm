#include "circularbuffer.h"
#define DEBUG
#include "emdesk/debug.h"

#include <errno.h>

#include <QMutex>
#include <QThread>

CircularBuffer::CircularBuffer(QObject *parent) :
	QObject(parent)
{
	mutex = new QMutex;
}

CircularBuffer::CircularBuffer(void *source, int size, QObject *parent) :
	QObject(parent)
{
	dataOwner = false;
	rawData = (char *)source;
	rawDataLen = size;
	mutex = new QMutex;
	lockThread = NULL;
	reset();
}

CircularBuffer::CircularBuffer(int size, QObject *parent) :
	QObject(parent)
{
	dataOwner = true;
	rawData = new char[size];
	rawDataLen = size;
	mutex = new QMutex;
	lockThread = NULL;
	reset();
}

CircularBuffer::~CircularBuffer()
{
	if (dataOwner && rawData) {
		delete rawData;
		rawData = NULL;
	}
	if (mutex) {
		delete mutex;
		mutex = NULL;
	}
}

void * CircularBuffer::getDataPointer()
{
	return tail;
}

int CircularBuffer::usedSize()
{
	return usedBufLen;
}

int CircularBuffer::freeSize()
{
	return freeBufLen;
}

int CircularBuffer::totalSize()
{
	return rawDataLen;
}

int CircularBuffer::useData(int size)
{
	if (size > usedBufLen)
		size = usedBufLen;
	tail += size;
	if (tail > rawData + rawDataLen) {
		tail -= rawDataLen;
		mDebug("tail passed end of circ buf, resetting");
	}
	freeBufLen += size;
	usedBufLen -= size;

	return size;
}

int CircularBuffer::addData(const void *data, int size)
{
	/* do not overwrite */
	if (size > freeBufLen)
		return -EINVAL;

	if (head + size > rawData + rawDataLen) {
		if (usedBufLen) {
			mDebug("no space left, shifting %d bytes out of %d bytes", usedBufLen, rawDataLen);
			/* At this point it is guarenteed that head > tail due to not overriding */
			/* TODO: Following memcpy may be optimized */
			memcpy(rawData, tail, usedBufLen);
			head = rawData + usedBufLen;
			tail = rawData;
		} else
			reset();
	}
	memcpy(head, data, size);
	head += size;
	freeBufLen -= size;
	usedBufLen += size;

	return 0;
}

int CircularBuffer::reset()
{
	freeBufLen = rawDataLen;
	head = tail = rawData;
	usedBufLen = 0;

	return 0;
}

void CircularBuffer::lock()
{
	if (lockThread == NULL)
		lockThread = QThread::currentThreadId();
	else if (lockThread == QThread::currentThreadId())
		mDebug("locking from thread %p twice, deadlock huh :)", QThread::currentThreadId());
	mutex->lock();
}

void CircularBuffer::unlock()
{
	lockThread = NULL;
	mutex->unlock();
}

