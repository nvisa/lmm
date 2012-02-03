#include "baselmmoutput.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "emdesk/debug.h"

#include <QTime>

#include <errno.h>

BaseLmmOutput::BaseLmmOutput(QObject *parent) :
	BaseLmmElement(parent)
{
	outputDelay = 0;
	doSync = true;
	fpsTiming = new QTime;
	fps = fpsBufferCount = 0;
	fpsTiming->start();
}

int BaseLmmOutput::start()
{
	outputDelay = 0;
	fps = fpsBufferCount = 0;
	fpsTiming->start();
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

int BaseLmmOutput::checkBufferTimeStamp(RawBuffer *buf, int jitter)
{
	int err = 0;
	if (streamTime->getStartTime() == 0) {
		streamTime->setStartTime(streamTime->getCurrentTime());
		streamTime->setStartPts(buf->getPts());
		mDebug("%s: setting stream start point", this->metaObject()->className());
	}
	qint64 encDelay = streamTime->getStartTime() - streamTime->getStartPts();
	qint64 rpts = buf->getPts();
	qint64 time = streamTime->getCurrentTime() - encDelay;

	if (outputDelay)
		jitter = outputDelay;

	qint64 rpts_j = rpts + jitter;
	if (rpts > 0) {
		if ((rpts < streamDuration || streamDuration < 0) && rpts_j >= time) {
			mInfo("it is not time to display buf %d, pts=%lld time=%lld",
				  buf->streamBufferNo(), rpts, time);
			err = -1;
		}
	}
	last_rpts = rpts;
	last_time = time;
	if (!err) {
		outputLatency = time - rpts;
		mDebug("%s: outputLatency=%lld outputDelay=%d time=%lld rpts=%lld runningTime=%lld fps=%d", this->metaObject()->className(),
			   outputLatency, outputDelay, time - streamTime->getStartPts(), rpts - streamTime->getStartPts(), streamTime->getFreeRunningTime(), fps);
		fpsBufferCount++;
		if (fpsTiming->elapsed() > 1000) {
			int elapsed = fpsTiming->restart();
			fps = fpsBufferCount * 1000 / elapsed;
			fpsBufferCount = 0;
		}
	}
	return err;
}

int BaseLmmOutput::outputBuffer(RawBuffer *)
{
	return 0;
}

int BaseLmmOutput::output()
{
	if (!inputBuffers.size())
		return -ENOENT;
	RawBuffer *buf = inputBuffers.first();
	if (checkBufferTimeStamp(buf)) {
		if (doSync)
			return -EBUSY;
		outputLatency = 0;
	}
	sentBufferCount++;
	inputBuffers.removeFirst();
	int err = outputBuffer(buf);
	delete buf;
	return err;
}
