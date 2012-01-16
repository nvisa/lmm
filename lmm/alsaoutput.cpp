#include "alsaoutput.h"
#include "alsa/alsa.h"
#include "rawbuffer.h"
#define DEBUG
#include "emdesk/debug.h"
#include <errno.h>

AlsaOutput::AlsaOutput(QObject *parent) :
	QObject(parent)
{
	alsaOut = new Alsa;
	alsaOut->open();
}

int AlsaOutput::addBuffer(RawBuffer *buffer)
{
	buffers << buffer;
	return 0;
}

int AlsaOutput::output()
{
	if (!buffers.size())
		return -ENOENT;
	RawBuffer *buf = buffers.takeFirst();
	const char *data = (const char *)buf->constData();
	alsaOut->write(data, buf->size());
	delete buf;
	return 0;
}
