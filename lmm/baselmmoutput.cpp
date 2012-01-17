#include "baselmmoutput.h"
#include "rawbuffer.h"
#define DEBUG
#include "emdesk/debug.h"

#include <QTime>

BaseLmmOutput::BaseLmmOutput(QObject *parent) :
	BaseLmmElement(parent)
{
}

int BaseLmmOutput::checkBufferTimeStamp(RawBuffer *buf, int jitter)
{
	qint64 rpts = buf->getPts();
	if (rpts > 0) {
		int pts = rpts / 1000;
		if (pts < streamDuration / 1000 && pts - jitter > streamTime->elapsed()) {
			mInfo("it is not time to display buf %d, pts=%d time=%d",
				  buf->streamBufferNo(), pts, streamTime->elapsed());
			return -1;
		}
	}
	return 0;
}
