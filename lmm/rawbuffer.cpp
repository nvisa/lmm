#include "rawbuffer.h"

#include <errno.h>

RawBuffer::RawBuffer(void *data, int size, QObject *parent) :
	QObject(parent)
{
	rawData = NULL;
	setSize(size);
	memcpy(rawData + prependPos, data, size);
}

RawBuffer::RawBuffer(QObject *parent) :
	QObject(parent)
{
	rawData = NULL;
}

RawBuffer::~RawBuffer()
{
	if (rawData)
		delete [] rawData;
}

void RawBuffer::setSize(int size)
{
	if (rawData)
		delete [] rawData;
	prependLen = prependPos = 4096;
	appendLen = 0;
	rawData = new char[prependLen + size + appendLen];
	rawDataLen = size;
}

int RawBuffer::prepend(const void *data, int size)
{
	if (prependPos < size)
		return -EINVAL;
	memcpy(rawData + prependPos - size, data, size);
	prependPos -= size;
	rawDataLen += size;
	return 0;
}

const void *RawBuffer::constData()
{
	return rawData + prependPos;
}

void *RawBuffer::data()
{
	return rawData + prependPos;
}
