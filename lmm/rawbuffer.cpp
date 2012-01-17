#include "rawbuffer.h"

#include <errno.h>

#include <QVariant>

RawBuffer::RawBuffer(void *data, int size, QObject *parent) :
	QObject(parent)
{
	refData = false;
	rawData = NULL;
	setSize(size);
	memcpy(rawData + prependPos, data, size);
}

RawBuffer::RawBuffer(QObject *parent) :
	QObject(parent)
{
	rawData = NULL;
	refData = false;
}

RawBuffer::~RawBuffer()
{
	if (rawData && !refData)
		delete [] rawData;
}

void RawBuffer::setRefData(void *data, int size)
{
	if (rawData && !refData)
		delete [] rawData;
	rawData = (char *)data;
	rawDataLen = size;
	refData = true;
	prependPos = 0;
}

void RawBuffer::addBufferParameter(QString par, QVariant val)
{
	parameters.insert(par, val);
}

QVariant RawBuffer::getBufferParameter(QString par)
{
	if (parameters.contains(par))
		return parameters[par];
	return QVariant();
}

void RawBuffer::setSize(int size)
{
	if (rawData && !refData)
		delete [] rawData;
	prependLen = prependPos = 4096;
	appendLen = 0;
	rawData = new char[prependLen + size + appendLen];
	rawDataLen = size;
}

int RawBuffer::prepend(const void *data, int size)
{
	if (refData)
		return -EPERM;
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
