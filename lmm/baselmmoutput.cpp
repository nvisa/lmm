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

qint64 BaseLmmOutput::getLatency()
{
	return getAvailableBufferTime() + outputLatency;
}

int BaseLmmOutput::checkBufferTimeStamp(RawBuffer *buf, int jitter)
{
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

	qint64 rpts_j = rpts - jitter;
	if (rpts > 0) {
		if ((rpts < streamDuration || streamDuration < 0) && rpts_j >= time) {
			mInfo("it is not time to display buf %d, pts=%lld time=%lld",
				  buf->streamBufferNo(), rpts, time);
			last_rpts = rpts;
			last_time = time;
			return -1;
		}
	}
	outputLatency = time -rpts;
	mDebug("%s: streamTime=%lld s_diff=%lld ts=%lld ts_diff=%lld sync_diff=%lld (delay=%d)", this->metaObject()->className(),
		   time, time - last_time, rpts, rpts - last_rpts, outputLatency, outputDelay);
	last_rpts = rpts;
	last_time = time;
	fpsBufferCount++;
	if (fpsTiming->elapsed() > 1000) {
		int elapsed = fpsTiming->restart();
		fps = fpsBufferCount * 1000 / elapsed;
		fpsBufferCount = 0;
	}
	return 0;
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
	}
	sentBufferCount++;
	inputBuffers.removeFirst();
	int err = outputBuffer(buf);
	delete buf;
	return err;
}
