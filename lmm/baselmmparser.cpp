#include "baselmmparser.h"

#include <lmm/debug.h>
#include <lmm/circularbuffer.h>

#include <errno.h>

BaseLmmParser::BaseLmmParser(QObject *parent) :
	BaseLmmElement(parent)
{
	circBuf = new CircularBuffer(4096 * 1024);
}

int BaseLmmParser::parseBlocking()
{
	int size = 4096 * 100;
	circBuf->lock();
	const uchar *data = (const uchar *)circBuf->getDataPointer();
	int avail = circBuf->usedSize();
	circBuf->unlock();
	if (avail < size) {
		/*
		 * Wait for place to become available in
		 * circular buffer.
		 */
		circBuf->wait(size);
		circBuf->lock();
		data = (const uchar *)circBuf->getDataPointer();
		circBuf->unlock();
	}

	shiftLock.lock();
	int err = parse(data, size);
	shiftLock.unlock();
	return err;
}

int BaseLmmParser::processBuffer(const RawBuffer &buf)
{
	circBuf->lock();
	int err = circBuf->addDataNoShift(buf.constData(), buf.size());
	if (err == -EINVAL) {
		circBuf->unlock();
		shiftLock.lock();
		circBuf->addData(buf.constData(), buf.size());
		shiftLock.unlock();
	} else if (err < -ENOSPC) {
		mDebug("waiting circular buffer for place");
		circBuf->unlock();
		circBuf->waitWr(buf.size());
		return processBuffer(buf);
	} else
		circBuf->unlock();
	return err;
}
