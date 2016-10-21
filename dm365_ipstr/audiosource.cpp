#include "audiosource.h"

#include <lmm/debug.h>
#include <lmm/lmmcommon.h>

#include <errno.h>
#include <unistd.h>

AudioSource::AudioSource(QObject *parent) :
	BaseLmmElement(parent)
{
}

static void generateCycle(short *data, int count)
{
	int q = count / 4;
	float step = 32768 / (float)q;
	for (int i = 0; i < q; i++)
		data[i] = qMin(int(i * step), 32767);
	for (int i = q; i < 2 * q; i++)
		data[i] = qMax(int(32767 - (i - q) * step), 0);
	for (int i = 2 * q; i < 3 * q; i++)
		data[i] = qMax(int(-1 * (i - 2 * q) * step), -32768);
	for (int i = 3 * q; i < 4 * q; i++)
		data[i] = qMin(int(-32768 + (i - 3 * q) * step), 0);
}

int AudioSource::processBlocking(int ch)
{
	Q_UNUSED(ch);

	int hz = 500;
	int sl = 1000;
	int srate = 8000;
	int channels = 1;
	int sformat = Lmm::AUDIO_SAMPLE_S16;
	int bps = 0;
	if (sformat == Lmm::AUDIO_SAMPLE_S16)
		bps = 2;

	int bytesPerSec = srate * channels * bps;
	int frameSize = bytesPerSec * sl / 1000;

	usleep(1000 * (sl - 25));
	if (isPassThru()) {
		sleep(1);
		return 0;
	}

	if (getState() == STOPPED)
		return -EINVAL;

	RawBuffer buf("audio/x-raw-int", frameSize);
	short *data = (short *)buf.constData();
	int cycles = hz * 1000 / sl;
	int sampleCount = frameSize / cycles / bps;
	generateCycle(data, sampleCount);
	for (int i = 1; i < cycles; i++)
		memcpy(data + i * sampleCount, data, sampleCount * bps);

	return newOutputBuffer(ch, buf);
}

int AudioSource::processBuffer(const RawBuffer &buf)
{
	Q_UNUSED(buf);
	return 0;
}
