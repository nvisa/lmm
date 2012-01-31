#include "baselmmelement.h"
#include "emdesk/debug.h"
#include "rawbuffer.h"

#include <errno.h>

#include <QTime>

BaseLmmElement::BaseLmmElement(QObject *parent) :
	QObject(parent)
{
	receivedBufferCount = sentBufferCount = 0;
}

int BaseLmmElement::addBuffer(RawBuffer *buffer)
{
	if (!buffer)
		return -EINVAL;
	inputBuffers << buffer;
	receivedBufferCount++;
	return 0;
}

RawBuffer * BaseLmmElement::nextBuffer()
{
	if (outputBuffers.size() == 0)
		return NULL;
	sentBufferCount++;
	return outputBuffers.takeFirst();
}

int BaseLmmElement::start()
{
	receivedBufferCount = sentBufferCount = 0;
	return 0;
}

int BaseLmmElement::stop()
{
	flush();
	return 0;
}

void BaseLmmElement::printStats()
{
	qDebug() << this << receivedBufferCount << sentBufferCount;
}

int BaseLmmElement::flush()
{
	qDeleteAll(inputBuffers.begin(), inputBuffers.end());
	qDeleteAll(outputBuffers.begin(), outputBuffers.end());
	inputBuffers.clear();
	outputBuffers.clear();
	return 0;
}
