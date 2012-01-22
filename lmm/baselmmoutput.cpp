#include "baselmmoutput.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "emdesk/debug.h"

BaseLmmOutput::BaseLmmOutput(QObject *parent) :
	BaseLmmElement(parent)
{
	outputDelay = 0;
	doSync = true;
}

int BaseLmmOutput::checkBufferTimeStamp(RawBuffer *buf, int jitter)
{
	if (!doSync)
		return 0;
	qint64 rpts = buf->getPts();
	qint64 time = streamTime->getCurrentTime();
	if (outputDelay)
		jitter = outputDelay;
	if (rpts > 0) {
		if (rpts < streamDuration && rpts - jitter > time) {
			mInfo("it is not time to display buf %d, pts=%lld time=%lld",
				  buf->streamBufferNo(), rpts, time);
			return -1;
		}
	}

	mDebug("%s: streamTime=%lld ts=%lld diff=%lld", this->metaObject()->className(),
		   time, rpts, time - rpts);
	return 0;
}
