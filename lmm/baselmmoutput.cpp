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
	dontDeleteBuffers = false;
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

/*
 * Sync logic:
 *
 * outputDelay:
 *	Used via setOutputDelay() causes delay in this output element.
 *	Used to adjust latency in different elements, for example audio latency
 *	is tolerated in video output elements using this variable.
 *
 * buf.pts + outputDelay
 *
 */
int BaseLmmOutput::checkBufferTimeStamp(RawBuffer buf, int jitter)
{
	if (!streamTime)
		return -ENOENT;
	int err = 0;
	if (streamTime->getStartTime() == 0) {
		streamTime->setStartTime(streamTime->getCurrentTime());
		streamTime->setStartPts(buf.getPts());
		mDebug("%s: setting stream start point", this->metaObject()->className());
	}
	/*
	 * encdelay represents time diff between global stream time and
	 * first output buffer's pts
	 */
	qint64 encDelay = streamTime->getStartTime() - streamTime->getStartPts();
	qint64 rpts = buf.getPts();
	/*
	 * time corresponds to current time relative to buffer pts
	 */
	qint64 time = streamTime->getCurrentTime() - encDelay;

	if (outputDelay)
		jitter = outputDelay;

	/*
	 * jitter is to tolerate delay in other elements
	 */
	qint64 rpts_j = rpts + jitter;
	if (rpts > 0) {
		if ((rpts < streamDuration || streamDuration < 0) && rpts_j >= time) {
			mInfo("it is not time to display buf %d, pts=%lld time=%lld",
				  buf.streamBufferNo(), rpts, time);
			err = -1;
		}
	}

	/*
	 * Following is for calculation output latency in this element
	 */
	last_rpts = rpts;
	last_time = time;
	if (!err) {
		outputLatency = time - rpts;
		if (doSync)
			mDebug("%s: outputLatency=%lld outputDelay=%d time=%lld rpts=%lld runningTime=%lld fps=%d",
			   this->metaObject()->className(),
			   outputLatency, outputDelay, time - streamTime->getStartPts(),
			   rpts - streamTime->getStartPts(),
			   streamTime->getFreeRunningTime(), getFps());
	}
	return err;
}

int BaseLmmOutput::outputBuffer(RawBuffer)
{
	return 0;
}

int BaseLmmOutput::outputFunc()
{
	if (!inputBuffers.size())
		return -ENOENT;
	RawBuffer buf = inputBuffers.first();
	if (checkBufferTimeStamp(buf)) {
		if (doSync) {
			inbufsem[0]->release();
			return -EBUSY;
		}
		outputLatency = 0;
	}
	sentBufferCount++;
	inputBuffers.removeFirst();
	int err = outputBuffer(buf);
	calculateFps();
	return err;
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
