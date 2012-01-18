#ifndef ALSA_H
#define ALSA_H

typedef struct _snd_pcm snd_pcm_t;
typedef unsigned long snd_pcm_uframes_t;

#include <QObject>
#include <QThread>
#include <QSemaphore>
#include <QMutex>
#include <QTime>

class Alsa : public QObject
{
	Q_OBJECT
public:
	Alsa(QObject *parent = NULL);
	int open();
	int close();
	int pause();
	int resume();
	int write(const void *buf, int length);
	/* return os delay in microseconds */
	int delay();
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

	int setHwParams();
	int setSwParams();
};

#endif // ALSA_H
