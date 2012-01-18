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
	alsaOut->open();
}

int AlsaOutput::output()
{
	if (!inputBuffers.size())
		return -ENOENT;
	RawBuffer *buf = inputBuffers.first();
	if (checkBufferTimeStamp(buf))
		return 0;
	inputBuffers.removeFirst();
	const char *data = (const char *)buf->constData();
	alsaOut->write(data, buf->size());
	delete buf;
	return 0;
}

int AlsaOutput::start()
{
	return alsaOut->open();
}

int AlsaOutput::stop()
{
	return alsaOut->close();
}

qint64 AlsaOutput::getLatency()
{
	return alsaOut->delay();
}
