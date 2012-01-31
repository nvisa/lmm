#include "baselmmplayer.h"
#include "baselmmdecoder.h"
#include "baselmmdemux.h"
#include "baselmmoutput.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "emdesk/debug.h"
#include "alsa/alsa.h"

#include <QTime>
#include <QTimer>

#include <errno.h>

BaseLmmPlayer::BaseLmmPlayer(QObject *parent) :
	QObject(parent)
{
	state = STOPPED;

	audioDecoder = NULL;
	videoDecoder = NULL;
	audioOutput = NULL;
	videoOutput = NULL;
	demux = NULL;
#ifdef CONFIG_ALSA
	alsaControl = NULL;
#endif

	live = false;
}

BaseLmmPlayer::~BaseLmmPlayer()
{
	qDeleteAll(elements);
	elements.clear();
}

int BaseLmmPlayer::play()
{
	if (state != STOPPED)
		return -EBUSY;

	state = RUNNING;

	streamTime = demux->getStreamTime(BaseLmmDemux::STREAM_VIDEO);
	streamTime->start();
	foreach (BaseLmmElement *el, elements) {
		if (demux && !live)
			el->setStreamDuration(demux->getTotalDuration());
		else
			el->setStreamDuration(-1);
		el->setStreamTime(streamTime);
		el->start();
	}

	timer = new QTimer(this);
	timer->setSingleShot(true);
	connect(timer, SIGNAL(timeout()), this, SLOT(decodeLoop()));
	timer->start(10);

	return 0;
}

int BaseLmmPlayer::stop()
{
	if (state != RUNNING)
		return -EBUSY;

	state = STOPPED;
	foreach (BaseLmmElement *el, elements)
		el->stop();

	return 0;
}

int BaseLmmPlayer::pause()
{
	return 0;
}

int BaseLmmPlayer::resume()
{
	return 0;
}

qint64 BaseLmmPlayer::getDuration()
{
	if (demux)
		return demux->getTotalDuration();
	return 0;
}

qint64 BaseLmmPlayer::getPosition()
{
	return streamTime->getCurrentTime();
}

int BaseLmmPlayer::seekTo(qint64 pos)
{
#ifdef CONFIG_ALSA
	if (alsaControl)
		alsaControl->mute(true);
#endif
	if (!demux->seekTo(pos)) {
		mDebug("seek ok");
		foreach (BaseLmmElement *el, elements)
			el->flush();
	}
	QTimer::singleShot(200, this, SLOT(audioPopTimerTimeout()));
	return 0;
}

int BaseLmmPlayer::seek(qint64 value)
{
	return seekTo(demux->getCurrentPosition() + value);
}

void BaseLmmPlayer::setMute(bool mute)
{
#ifdef CONFIG_ALSA
	if (alsaControl)
		alsaControl->mute(mute);
#endif
}

void BaseLmmPlayer::setVolumeLevel(int per)
{
#ifdef CONFIG_ALSA
	if (alsaControl)
		alsaControl->setCurrentVolumeLevel(per);
#endif
}

int BaseLmmPlayer::getVolumeLevel()
{
#ifdef CONFIG_ALSA
	if (alsaControl)
		return alsaControl->currentVolumeLevel();
	return 0;
#else
	return -ENOENT;
#endif
}

void BaseLmmPlayer::decodeLoop()
{
	if (state != RUNNING) {
		mDebug("we are not in a running state");
		return;
	}

	int err = demux->demuxOne();
	if (!err) {
		if (audioDecoder) {
			audioLoop();
			qint64 lat = audioOutput->getLatency();
			while (lat < 50000) {
				audioLoop();
				lat = audioOutput->getLatency();
				if (audioOutput->getInputBufferCount() == 0 && demux->audioBufferCount() == 0)
					break;
			}
			if (videoOutput)
				videoOutput->setOutputDelay(lat);
		}
		if (videoDecoder)
			videoLoop();
	} else if (err == -ENOENT) {
		mDebug("decoding finished");
		stop();
	}
	timer->start(5);
}

void BaseLmmPlayer::audioPopTimerTimeout()
{
#ifdef CONFIG_ALSA
	if (alsaControl)
		alsaControl->mute(false);
#endif
}

void BaseLmmPlayer::audioLoop()
{
	RawBuffer *buf = demux->nextAudioBuffer();
	if (buf)
		audioDecoder->addBuffer(buf);
	if (audioDecoder->decode())
		mDebug("audio decoder reported error");
	buf = audioDecoder->nextBuffer();
	if (buf)
		audioOutput->addBuffer(buf);
	audioOutput->output();
}

void BaseLmmPlayer::videoLoop()
{
	RawBuffer *buf = demux->nextVideoBuffer();
	if (buf)
		videoDecoder->addBuffer(buf);
	videoDecoder->decode();
	buf = videoDecoder->nextBuffer();
	if (buf)
		videoOutput->addBuffer(buf);
	videoOutput->output();
}
