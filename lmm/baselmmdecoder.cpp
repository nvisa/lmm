#include "baselmmdecoder.h"
#include "rawbuffer.h"
#include "debug.h"

#include <errno.h>

BaseLmmDecoder::BaseLmmDecoder(QObject *parent) :
	BaseLmmElement(parent)
{
}

int BaseLmmDecoder::start()
{
	if (inTimeStamps.size()) {
		qDeleteAll(inTimeStamps);
		inTimeStamps.clear();
	}
	timestamp = NULL;
	int err = startDecoding();
	if (err)
		return err;
	return BaseLmmElement::start();
}

int BaseLmmDecoder::stop()
{
	int err = stopDecoding();
	if (err)
		return err;
	return BaseLmmElement::stop();
}

int BaseLmmDecoder::flush()
{
	if (inTimeStamps.size()) {
		qDeleteAll(inTimeStamps);
		inTimeStamps.clear();
	}
	timestamp = NULL;
	return BaseLmmElement::flush();
}

void BaseLmmDecoder::handleInputTimeStamps(RawBuffer *buf)
{
	qint64 pts = buf->getPts();
	if (pts != -1) {
		decodeTimeStamp *ts = new decodeTimeStamp;
		ts->pts = pts;
		ts->duration = buf->getDuration();
		ts->validDuration = buf->getDuration();
		ts->usedDuration = 0;
		/*
		 * Following timestamp operation is a little bit tricky
		 * and hacky. ffmpeg demuxer outputs video frames in well defined
		 * order and one frame at a time. For instance, this may be what you
		 * get for an MPEG-2 stream from ffmpeg:
		 *
		 *  I -- P -- P -- P -- P -- B -- B -- P -- P -- I ...
		 *
		 * You get a well defined timestamp for each frame in the above
		 * figure as well. Please note that dts'es of these frames are in
		 * ascending order whereas pts'es are not due to B frames presence.
		 *
		 * Our decoding implementation loses this information so when we use
		 * pts'es in the same order coming from ffmpeg demuxer, at the output
		 * we see a non-monotonic pts flow. Here we workaround this situation
		 * instead of fixing our decoding implementation. We re-order pts'es
		 * to be monotonic so that output is monotonic and pray for a perfect
		 * sync with demuxer and decoder :(
		 *
		 * TODO: Fix inappropriate decoder implementation regarding timestamps.
		 */
		if (inTimeStamps.size() == 0) {
			inTimeStamps << ts;
			mInfo("first timestamp: pts=%lld duration=%d", ts->pts, ts->duration);
		} else {
			int i = inTimeStamps.size();
			int lastIndex = -1;
			if (timestamp)
				lastIndex = inTimeStamps.indexOf(timestamp);
			decodeTimeStamp *last = inTimeStamps.last();
			while (last->pts > ts->pts) {
				if (i < lastIndex)
					/* invalidate old timestamp because we have a recent one */
					timestamp = NULL;
				i--;
				if (i == 0)
					break;
				last = inTimeStamps.at(i - 1);
			}
			inTimeStamps.insert(i, ts);
			mInfo("adding new timestamp %lld at index %d", ts->pts, i);
		}
		/* set timestamp if it is not already */
		if (!timestamp)
			timestamp = ts;
	} else {
		mInfo("got invalid pts, using previous");
		/* increase validty of last ts */
		decodeTimeStamp *ts;
		if (!inTimeStamps.size()) {
			mDebug("no last ts present");
		} else {
			ts = inTimeStamps.last();
			ts->validDuration += buf->getDuration();
		}
	}
}

void BaseLmmDecoder::setOutputTimeStamp(RawBuffer *buf, int minDuration)
{
	if (timestamp) {
		if (timestamp->validDuration - timestamp->usedDuration < minDuration) {
			/* our timestamp is not valid anymore */
			if (inTimeStamps.size()) {
				inTimeStamps.removeAll(timestamp);
				delete timestamp;
				timestamp = NULL;
			}
			if (inTimeStamps.size())
				timestamp = inTimeStamps.first();
			else {
				mDebug("timestamp is not valid and we don't have more");
				return;
			}
			mInfo("using new timestamp %lld, used duration is %d", timestamp->pts, timestamp->usedDuration);
		} else
			mInfo("using old timestamp");
		buf->setPts(timestamp->pts + timestamp->usedDuration);
		buf->setDuration(timestamp->duration);
		timestamp->usedDuration += timestamp->duration;
	} else {
		mDebug("no timestamp");
		buf->setPts(-1);
		buf->setDuration(-1);
	}
}

int BaseLmmDecoder::processBuffer(RawBuffer buf)
{
	return decode(buf);
}
