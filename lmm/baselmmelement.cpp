#include "baselmmelement.h"
#include "emdesk/debug.h"
#include "rawbuffer.h"

#include <errno.h>

#include <QTime>

BaseLmmElement::BaseLmmElement(QObject *parent) :
	QObject(parent)
{
	state = INIT;
	threaded = false;
	receivedBufferCount = sentBufferCount = 0;
	streamTime = NULL;
	fpsTiming = new QTime;
	elementFps = fpsBufferCount = 0;
	fpsTiming->start();
}

int BaseLmmElement::addBuffer(RawBuffer buffer)
{
	if (buffer.size() == 0)
		return -EINVAL;
	if (threaded)
		inputLock.lock();
	inputBuffers << buffer;
	if (threaded)
		inputLock.unlock();
	receivedBufferCount++;
	return 0;
}

RawBuffer BaseLmmElement::nextBuffer()
{
	RawBuffer buf;
	if (threaded)
		outputLock.lock();
	if (outputBuffers.size() != 0)
		buf = outputBuffers.takeFirst();
	if (threaded)
		outputLock.unlock();
	if (buf.size()) {
		sentBufferCount++;
		calculateFps();
	}

	return buf;
}

int BaseLmmElement::start()
{
	receivedBufferCount = sentBufferCount = 0;
	elementFps = fpsBufferCount = 0;
	fpsTiming->start();
	state = STARTED;
	return 0;
}

int BaseLmmElement::stop()
{
	state = STOPPED;
	flush();
	return 0;
}

void BaseLmmElement::printStats()
{
	qDebug() << this << receivedBufferCount << sentBufferCount;
}

void BaseLmmElement::calculateFps()
{
	fpsBufferCount++;
	if (fpsTiming->elapsed() > 1000) {
		int elapsed = fpsTiming->restart();
		elementFps = fpsBufferCount * 1000 / elapsed;
		fpsBufferCount = 0;
	}
}

BaseLmmElement::RunningState BaseLmmElement::getState()
{
	return state;
}

int BaseLmmElement::flush()
{
	inputLock.lock();
	inputBuffers.clear();
	inputLock.unlock();
	outputLock.lock();
	outputBuffers.clear();
	outputLock.unlock();
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

int BaseLmmElement::setThreaded(bool)
{
	threaded = true;
	return 0;
}
