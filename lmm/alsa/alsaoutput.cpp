#include "alsaoutput.h"
#include "alsa/alsa.h"
#include "rawbuffer.h"
#define DEBUG
#include "debug.h"
#include <errno.h>

AlsaOutput::AlsaOutput(QObject *parent) :
	BaseLmmOutput(parent)
{
	alsaOut = new Alsa;
	unmute = false;
}

int AlsaOutput::outputBuffer(RawBuffer buf)
{
	const char *data = (const char *)buf.constData();
	alsaOut->write(data, buf.size());
	if (unmute) {
		alsaOut->mute(false);
		unmute = false;
	}
	return 0;
}

int AlsaOutput::start()
{
	/* alsa defaults 48000 for invalid sample rates */
	int err = alsaOut->open(getParameter("audioRate").toInt());
	if (err)
		return err;
	return BaseLmmOutput::start();
}

int AlsaOutput::stop()
{
	int err = alsaOut->close();
	if (err)
		return err;
	return BaseLmmOutput::stop();
}

int AlsaOutput::flush()
{
	muteTillFirstOutput();
	return BaseLmmOutput::flush();
}

int AlsaOutput::setParameter(QString param, QVariant value)
{
	int err = BaseLmmElement::setParameter(param, value);
	if (err)
		return err;
	if (param == "audioRate") {
		alsaOut->close();
		alsaOut->open(value.toInt());
	}
	return 0;
}

qint64 AlsaOutput::getAvailableBufferTime()
{
	qint64 delay = alsaOut->delay();
	if (delay < 10000) {
		mDebug("audio buffer will underrun, check for slow decoding ???");
	}
	return delay;
}

int AlsaOutput::muteTillFirstOutput()
{
	alsaOut->mute(true);
	unmute = true;
	return 0;
}
