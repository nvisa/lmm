#include "rawbuffer.h"
#include "baselmmelement.h"

#include <errno.h>

#include <QVariant>

RawBuffer::RawBuffer(void *data, int size, BaseLmmElement *parent) :
	QObject(parent)
{
	refData = false;
	rawData = NULL;
	setSize(size);
	memcpy(rawData + prependPos, data, size);
	myParent = parent;
}

RawBuffer::RawBuffer(int size, BaseLmmElement *parent) :
	QObject(parent)
{
	refData = false;
	rawData = NULL;
	setSize(size);
	myParent = parent;
}

RawBuffer::RawBuffer(BaseLmmElement *parent) :
	QObject(parent)
{
	rawData = NULL;
	refData = false;
	myParent = parent;
}

RawBuffer::~RawBuffer()
{
	if (rawData && !refData)
		delete [] rawData;
	if (myParent)
		myParent->aboutDeleteBuffer(this);
}

void RawBuffer::setRefData(void *data, int size)
{
	if (rawData && !refData)
		delete [] rawData;
	rawData = (char *)data;
	rawDataLen = size;
	refData = true;
	prependPos = 0;
	usedLen = size;
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
	usedLen = size;
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

int RawBuffer::setUsedSize(int size)
{
	if (size > rawDataLen)
		return -EINVAL;
	usedLen = size;
	return 0;
}
