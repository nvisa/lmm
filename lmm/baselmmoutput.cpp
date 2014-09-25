#include "baselmmoutput.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "debug.h"
#include "lmmthread.h"

#include <QThread>

#include <errno.h>

BaseLmmOutput::BaseLmmOutput(QObject *parent) :
	BaseLmmElement(parent)
{
	outputDelay = 0;
	doSync = true;
	lastOutputPts = 0;
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

int BaseLmmOutput::processBuffer(const RawBuffer &buf)
{
	int err = outputBuffer(buf);
	lastOutputPts = buf.constPars()->pts;
	return err;
}

int BaseLmmOutput::outputBuffer(RawBuffer)
{
	return 0;
}
