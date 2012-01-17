#include "circularbuffer.h"

#include <errno.h>
#include <QDebug>

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
	if (tail > rawData + rawDataLen)
		tail -= rawDataLen;
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
		/* At this point it is guarenteed thah head > tail due to not overriding */
		memcpy(tail, rawData, usedBufLen);
		head = rawData + usedBufLen;
		tail = rawData;
	}
	memcpy(head, data, size);
	head += size;
	freeBufLen -= size;
	usedBufLen += size;
	return 0;
}

