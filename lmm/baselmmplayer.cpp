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
	streamTime = new StreamTime;
	state = STOPPED;

	audioDecoder = NULL;
	videoDecoder = NULL;
	audioOutput = NULL;
	videoOutput = NULL;
	demux = NULL;
	alsaControl = NULL;
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

	foreach (BaseLmmElement *el, elements) {
		el->start();
		if (demux)
			el->setStreamDuration(demux->getTotalDuration());
		el->setStreamTime(streamTime);
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
	if (alsaControl)
		alsaControl->mute(true);
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
	if (alsaControl)
		alsaControl->mute(mute);
}

void BaseLmmPlayer::setVolumeLevel(int per)
{
	if (alsaControl)
		alsaControl->setCurrentVolumeLevel(per);
}

int BaseLmmPlayer::getVolumeLevel()
{
	if (alsaControl)
		return alsaControl->currentVolumeLevel();
	return 0;
}

void BaseLmmPlayer::decodeLoop()
{
	if (state != RUNNING) {
		mDebug("we are not in a running state");
		return;
	}

	int err = demux->demuxOne();
	if (!err) {
		streamTime->setCurrentTime(demux->getCurrentPosition());
		if (audioDecoder)
			audioLoop();
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
	if (alsaControl)
		alsaControl->mute(false);
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
	if (audioOutput)
		videoOutput->setOutputDelay(audioOutput->getLatency());
	RawBuffer *buf = demux->nextVideoBuffer();
	if (buf)
		videoDecoder->addBuffer(buf);
	videoDecoder->decode();
	buf = videoDecoder->nextBuffer();
	if (buf)
		videoOutput->addBuffer(buf);
	videoOutput->output();
}
