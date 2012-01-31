#include "alsaoutput.h"
#include "alsa/alsa.h"
#include "rawbuffer.h"
#define DEBUG
#include "emdesk/debug.h"
#include <errno.h>

AlsaOutput::AlsaOutput(QObject *parent) :
	BaseLmmOutput(parent)
{
	alsaOut = new Alsa;
}

int AlsaOutput::outputBuffer(RawBuffer *buf)
{
	const char *data = (const char *)buf->constData();
	alsaOut->write(data, buf->size());
	return 0;
}

int AlsaOutput::start()
{
	int err = alsaOut->open();
	if (err)
		return err;
	return BaseLmmElement::start();
}

int AlsaOutput::stop()
{
	int err = alsaOut->close();
	if (err)
		return err;
	return BaseLmmElement::stop();
}

int AlsaOutput::flush()
{
	return BaseLmmOutput::flush();
}

qint64 AlsaOutput::getAvailableBufferTime()
{
	qint64 delay = alsaOut->delay();
	if (delay < 10000) {
		mDebug("audio buffer will underrun, check for slow decoding ???");
	}
	return delay;
}
