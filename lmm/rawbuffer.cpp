#include "rawbuffer.h"
#include "baselmmelement.h"

#include <errno.h>

#include <QVariant>

RawBuffer::RawBuffer(void *data, int size, BaseLmmElement *parent)
{
	d = new RawBufferData;
	d->refData = false;
	d->rawData = NULL;
	setSize(size);
	memcpy(d->rawData + d->prependPos, data, size);
	d->myParent = parent;
}

RawBuffer::RawBuffer(int size, BaseLmmElement *parent)
{
	d = new RawBufferData;
	d->refData = false;
	d->rawData = NULL;
	setSize(size);
	d->myParent = parent;
}

RawBuffer::RawBuffer(const RawBuffer &other)
	: d (other.d)
{
}

void RawBuffer::setParentElement(BaseLmmElement *el)
{
	 d->myParent = el;
}

RawBuffer::RawBuffer(BaseLmmElement *parent)
{
	d = new RawBufferData;
	d->myParent = parent;
}

RawBuffer::~RawBuffer()
{
}

void RawBuffer::setRefData(void *data, int size)
{
	if (d->rawData && !d->refData)
		delete [] d->rawData;
	d->rawData = (char *)data;
	d->rawDataLen = size;
	d->refData = true;
	d->prependPos = 0;
	d->usedLen = size;
}

void RawBuffer::addBufferParameter(QString par, QVariant val)
{
	d->parameters.insert(par, val);
}

QVariant RawBuffer::getBufferParameter(QString par)
{
	if (d->parameters.contains(par))
		return d->parameters[par];
	return QVariant();
}

void RawBuffer::setSize(int size)
{
	if (d->rawData && !d->refData)
		delete [] d->rawData;
	d->prependLen = d->prependPos = 4096;
	d->appendLen = 0;
	d->rawData = new char[d->prependLen + size + d->appendLen];
	d->rawDataLen = size;
	d->usedLen = size;
}

int RawBuffer::prepend(const void *data, int size)
{
	if (d->refData)
		return -EPERM;
	if (d->prependPos < size)
		return -EINVAL;
	memcpy(d->rawData + d->prependPos - size, data, size);
	d->prependPos -= size;
	d->rawDataLen += size;
	return 0;
}

const void *RawBuffer::constData()
{
	return d->rawData + d->prependPos;
}

void *RawBuffer::data()
{
	return d->rawData + d->prependPos;
}

int RawBuffer::size()
{
	return d->usedLen;
}

int RawBuffer::setUsedSize(int size)
{
	if (size > d->rawDataLen)
		return -EINVAL;
	d->usedLen = size;
	return 0;
}

void RawBuffer::setDuration(unsigned int val)
{
	d->duration = val;
}

unsigned int RawBuffer::getDuration()
{
	return d->duration;
}

void RawBuffer::setPts(qint64 val)
{
	d->pts = val;
}

qint64 RawBuffer::getPts()
{
	return d->pts;
}

void RawBuffer::setDts(qint64 val)
{
	d->dts = val;
}

qint64 RawBuffer::getDts()
{
	return d->dts;
}

void RawBuffer::setStreamBufferNo(int val)
{
	d->bufferNo = val;
}

int RawBuffer::streamBufferNo()
{
	return d->bufferNo;
}

RawBufferData::~RawBufferData()
{
	if (rawData && !refData)
		delete [] rawData;
	if (myParent)
		myParent->aboutDeleteBuffer(parameters);
}
