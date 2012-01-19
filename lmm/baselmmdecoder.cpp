#include "baselmmdecoder.h"
#include "rawbuffer.h"
#define DEBUG
#include "emdesk/debug.h"

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
	return BaseLmmElement::start();
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
		inTimeStamps << ts;
		if (!timestamp)
			timestamp = ts;
	} else {
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
				inTimeStamps.removeFirst();
				delete timestamp;
			}
			if (inTimeStamps.size())
				timestamp = inTimeStamps.first();
			else
				mDebug("timestamp is not valid and we don't have more");
		}
		buf->setPts(timestamp->pts + timestamp->usedDuration);
		buf->setDuration(timestamp->duration);
		timestamp->usedDuration += timestamp->duration;
	} else {
		buf->setPts(-1);
		buf->setDuration(-1);
	}
}
