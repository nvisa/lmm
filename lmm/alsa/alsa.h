#ifndef ALSA_H
#define ALSA_H

typedef struct _snd_pcm snd_pcm_t;
typedef unsigned long snd_pcm_uframes_t;

#include <QObject>
#include <QThread>
#include <QSemaphore>
#include <QMutex>
#include <QTime>

#include <alsa/input.h>
#include <alsa/output.h>
#include <alsa/conf.h>
#include <alsa/global.h>
#include <alsa/control.h>

class Alsa : public QObject
{
	Q_OBJECT
public:
	Alsa(QObject *parent = NULL);
	int open(int rate = 48000);
	int close();
	int pause();
	int resume();
	int write(const void *buf, int length);
	int mute(bool mute);
	/* return os delay in microseconds */
	int delay();

	int currentVolumeLevel();
	int setCurrentVolumeLevel(int per);
private:
	snd_pcm_t *handle;
	snd_pcm_uframes_t bufferSize;
	snd_pcm_uframes_t periodSize;
	unsigned int bufferTime;
	unsigned int periodTime;
	int bytesPerSample;
	QMutex mutex;
	bool running;
	int sampleRate;
	int driverDelay;

	snd_hctl_t *hctl;
	snd_hctl_elem_t *mixerVolumeElem;
	snd_ctl_elem_value_t *mixerVolumeControl;
	snd_ctl_elem_id_t *mixerVolumeElemId;
	snd_hctl_elem_t *mixerSwitchElem;
	snd_ctl_elem_value_t *mixerSwitchControl;
	snd_ctl_elem_id_t *mixerSwitchElemId;

	int setHwParams(int rate);
	int setSwParams();
	void initVolumeControl();
	int readDriverDelay();
};

#endif // ALSA_H
