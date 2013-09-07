#include "baselmmoutput.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "debug.h"
#include "lmmthread.h"

#include <QTime>
#include <QThread>

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

int BaseLmmOutput::processBuffer(RawBuffer buf)
{
	int err = outputBuffer(buf);
	return err;
}

int BaseLmmOutput::outputBuffer(RawBuffer)
{
	return 0;
}
