#include "baselmmelement.h"
#include "emdesk/debug.h"
#include "rawbuffer.h"

#include <errno.h>

#include <QTime>

BaseLmmElement::BaseLmmElement(QObject *parent) :
	QObject(parent)
{
	receivedBufferCount = sentBufferCount = 0;
	streamTime = NULL;
	fpsTiming = new QTime;
	elementFps = fpsBufferCount = 0;
	fpsTiming->start();
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
	fpsBufferCount++;
	if (fpsTiming->elapsed() > 1000) {
		int elapsed = fpsTiming->restart();
		elementFps = fpsBufferCount * 1000 / elapsed;
		fpsBufferCount = 0;
	}
	return outputBuffers.takeFirst();
}

int BaseLmmElement::start()
{
	receivedBufferCount = sentBufferCount = 0;
	elementFps = fpsBufferCount = 0;
	fpsTiming->start();
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

int BaseLmmElement::setParameter(QString param, QVariant value)
{
	parameters.insert(param, value);
	return 0;
}

QVariant BaseLmmElement::getParameter(QString param)
{
	if (parameters.contains(param))
		return parameters[param];
	return QVariant();
}
