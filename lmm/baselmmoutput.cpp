#include "baselmmoutput.h"
#include "rawbuffer.h"
#include "emdesk/debug.h"

#include <QTime>

BaseLmmOutput::BaseLmmOutput(QObject *parent) :
	BaseLmmElement(parent)
{
}

int BaseLmmOutput::checkBufferTimeStamp(RawBuffer *buf, int jitter)
{
	qint64 rpts = buf->getPts();
	int pts = rpts / 1000;
	int time = streamTime->elapsed();
	if (rpts > 0) {
		if (pts < streamDuration / 1000 && pts - jitter > time) {
			mInfo("it is not time to display buf %d, pts=%d time=%d",
				  buf->streamBufferNo(), pts, time);
			return -1;
		}
	}

	mDebug("%s: streamTime=%d ts=%d diff=%d", this->metaObject()->className(),
		   time, pts, time - pts);
	return 0;
}
