#include "baselmmelement.h"
#include "emdesk/debug.h"
#include "rawbuffer.h"

#include <errno.h>

#include <QTime>

BaseLmmElement::BaseLmmElement(QObject *parent) :
	QObject(parent)
{
	inputBufferCount = outputBufferCount = 0;
}

int BaseLmmElement::addBuffer(RawBuffer *buffer)
{
	if (!buffer)
		return -EINVAL;
	inputBuffers << buffer;
	inputBufferCount++;
	return 0;
}

RawBuffer * BaseLmmElement::nextBuffer()
{
	if (outputBuffers.size() == 0)
		return NULL;
	outputBufferCount++;
	return outputBuffers.takeFirst();
}

void BaseLmmElement::printStats()
{
	qDebug() << this << inputBufferCount << outputBufferCount;
}

int BaseLmmElement::flush()
{
	qDeleteAll(inputBuffers.begin(), inputBuffers.end());
	qDeleteAll(outputBuffers.begin(), outputBuffers.end());
	inputBuffers.clear();
	outputBuffers.clear();
	return 0;
}
