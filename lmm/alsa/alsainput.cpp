#include "alsainput.h"
#include "debug.h"
#include "alsa.h"

#include <errno.h>

AlsaInput::AlsaInput(QObject *parent) :
	BaseLmmElement(parent)
{
	alsaIn = new Alsa;
}

int AlsaInput::processBlocking(int ch)
{
	Q_UNUSED(ch);
	if (getState() == STOPPED)
		return -EINVAL;
	RawBuffer buf("audio/x-raw-int,", bufferSize * 4);
	int err = alsaIn->read(buf.data(), bufferSize);
	if (getState() == STOPPED)
		return -EINVAL;
	if (err < 0)
		return err;
	return newOutputBuffer(ch, buf);
}

int AlsaInput::processBuffer(RawBuffer buf)
{
	Q_UNUSED(buf);
	return 0;
}

int AlsaInput::start()
{
	bufferSize = getParameter("sampleSize").toInt() * 2 * 2;
	/* alsa defaults 48000 for invalid sample rates */
	int err = alsaIn->open(getParameter("audioRate").toInt(), true);
	if (err)
		return err;
	return BaseLmmElement::start();
}

int AlsaInput::stop()
{
	int err = alsaIn->close();
	if (err)
		return err;
	return BaseLmmElement::stop();
}
