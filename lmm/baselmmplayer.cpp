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
#include <QtConcurrentRun>

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
	QTime time; time.restart();
	int dTime, aTime, vTime;
	if (state != RUNNING) {
		mDebug("we are not in a running state");
		return;
	}

	dTime = time.elapsed();
	int err = demux->demuxOne();
	dTime = time.elapsed() - dTime;
	if (!err) {
		if (audioDecoder) {
			aTime = time.elapsed();
			audioLoop();
			aTime = time.elapsed() - aTime;

			if (videoOutput)
				videoOutput->setOutputDelay(audioOutput->getLatency());
		} else
			demux->setAudioDemuxing(false);
		vTime = time.elapsed();
		if (videoDecoder)
			videoLoop();
		else
			demux->setVideoDemuxing(false);
		vTime = time.elapsed() - vTime;
	} else if (err == -ENOENT) {
		mDebug("decoding finished");
		stop();
	}
	mInfo("loop time=%d demux=%d audio=%d video=%d", time.elapsed(), dTime, aTime, vTime);
	timer->start(1);
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
	if (!videoDecodeFuture.isFinished()) {
		mInfo("previous decode operation is not finished");
		return;
	}
	RawBuffer *buf = demux->nextVideoBuffer();
	if (buf)
		videoDecoder->addBuffer(buf);
	videoDecodeFuture = QtConcurrent::run(videoDecoder, &BaseLmmDecoder::decode);
	buf = videoDecoder->nextBuffer();
	if (buf)
		videoOutput->addBuffer(buf);
	videoOutput->output();
}
