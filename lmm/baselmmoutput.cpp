#include "baselmmoutput.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "debug.h"
#include "lmmthread.h"

#include <QTime>
#include <QThread>
#include <QSemaphore>

#include <errno.h>

BaseLmmOutput::BaseLmmOutput(QObject *parent) :
	BaseLmmElement(parent)
{
	outputDelay = 0;
	doSync = true;
}

int BaseLmmOutput::start()
{
	outputDelay = 0;
	return BaseLmmElement::start();
}

int BaseLmmOutput::stop()
{
	return BaseLmmElement::stop();
}

qint64 BaseLmmOutput::getLatency()
{
	return getAvailableBufferTime() + outputLatency;
}

int BaseLmmOutput::getLoopLatency()
{
	/* TODO: Implement default loop latency calculation */
	return 0;
}

int BaseLmmOutput::outputBuffer(RawBuffer)
{
	return 0;
}

int BaseLmmOutput::output()
{
	inputLock.lock();
	RawBuffer buf = inputBuffers.takeFirst();
	sentBufferCount++;
	inputLock.unlock();
	int err = outputBuffer(buf);
	calculateFps();
	return err;
}

int BaseLmmOutput::outputBlocking()
{
	inbufsem[0]->acquire();
	return output();
}
