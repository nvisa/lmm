#include "alsaoutput.h"
#include "alsa/alsa.h"
#include "rawbuffer.h"
#include "debug.h"
#include <errno.h>

AlsaOutput::AlsaOutput(QObject *parent) :
	BaseLmmOutput(parent)
{
	alsaOut = new Alsa;
	unmute = false;
	channels = 2;
	format = Lmm::AUDIO_SAMPLE_S16;
	audioRate = 48000;
}

int AlsaOutput::outputBuffer(RawBuffer buf)
{
	if (!alsaOut->isOpen()) {
		/* alsa defaults to 48000 for invalid sample rates */
		QStringList mime = buf.getMimeType().split(",");
		foreach (QString pair, mime) {
			if (!pair.contains("="))
				continue;
			QStringList flds = pair.split("=");
			if (flds[0] == "rate")
				audioRate = flds[1].toInt();
			if (flds[0] == "fmt")
				format = (Lmm::AudioSampleType)flds[1].toInt();
			if (flds[0] == "channels")
				channels = flds[1].toInt();
		}
		int err = alsaOut->open(audioRate, channels, format);
		if (err)
			return err;
	}

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
	channels = getParameter("channelCount").toInt();
	audioRate = getParameter("audioRate").toInt();
	//bufferSize = getParameter("sampleCount").toInt() * chCount * alsaIn->getSampleSize();

	/* we will open alsa device with first buffer */
	alsaOut->close();
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
	if (isRunning() && param == "audioRate") {
		audioRate = value.toInt();
		alsaOut->close();
		alsaOut->open(audioRate, channels, format);
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
	return 0;
	alsaOut->mute(true);
	unmute = true;
	return 0;
}
