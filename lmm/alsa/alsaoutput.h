#ifndef ALSAOUTPUT_H
#define ALSAOUTPUT_H

#include <lmm/lmmcommon.h>
#include <lmm/baselmmoutput.h>

#include <QList>

class Alsa;
class RawBuffer;
class BaseLmmElement;

class AlsaOutput : public BaseLmmOutput
{
	Q_OBJECT
public:
	explicit AlsaOutput(QObject *parent = 0);
	int outputBuffer(RawBuffer buf);
	int start();
	int stop();
	int flush();
	int setParameter(QString param, QVariant value);
	Alsa * alsaControl() { return alsaOut; }
	qint64 getAvailableBufferTime();

	int muteTillFirstOutput();

	int getAudioRate() { return audioRate; }
	int getAudioChannels() { return channels; }
	Lmm::AudioSampleType getAudioFormat() { return format; }
signals:
	
public slots:
private:
	Alsa *alsaOut;
	bool unmute;
	int channels;
	int audioRate;
	Lmm::AudioSampleType format;
};

#endif // ALSAOUTPUT_H
