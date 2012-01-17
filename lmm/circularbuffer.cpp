#include "circularbuffer.h"
#define DEBUG
#include "emdesk/debug.h"

#include <errno.h>

CircularBuffer::CircularBuffer(QObject *parent) :
	QObject(parent)
{
}

CircularBuffer::CircularBuffer(void *source, int size, QObject *parent) :
	QObject(parent)
{
	rawData = (char *)source;
	rawDataLen = freeBufLen = size;
	head = tail = rawData;
	usedBufLen = 0;
}

void * CircularBuffer::getDataPointer()
{
	return tail;
}

int CircularBuffer::usedSize()
{
	return usedBufLen;
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
		mDebug("no space left, shifting");
		/* At this point it is guarenteed that head > tail due to not overriding */
		/* TODO: Following memcpy may be optimized */
		memcpy(rawData, tail, usedBufLen);
		head = rawData + usedBufLen;
		tail = rawData;
	}
	memcpy(head, data, size);
	head += size;
	freeBufLen -= size;
	usedBufLen += size;

	return 0;
}

