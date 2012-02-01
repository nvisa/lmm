#include "baselmmplayer.h"
#include "baselmmdecoder.h"
#include "baselmmdemux.h"
#include "baselmmoutput.h"
#include "rawbuffer.h"
#include "circularbuffer.h"
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

	mainSource = NULL;
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

int BaseLmmPlayer::play(QString url)
{
	if (state != STOPPED)
		return -EBUSY;

	mInfo("starting playback");

	connect(demux, SIGNAL(streamInfoFound()), SLOT(streamInfoFound()));
	int err = demux->setSource(url);
	if (err)
		return err;
	state = RUNNING;

	streamTime = demux->getStreamTime(BaseLmmDemux::STREAM_VIDEO);
	streamTime->start();
	foreach (BaseLmmElement *el, elements) {
		el->setStreamTime(streamTime);
		el->start();
		mInfo("started element %s", el->metaObject()->className());
	}

	mInfo("all elements started, game is a foot");
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

	disconnect(demux, SIGNAL(streamInfoFound()));
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

int BaseLmmPlayer::decodeLoop()
{
	QTime time; time.restart();
	int dTime, aTime, vTime;
	if (state != RUNNING) {
		mDebug("we are not in a running state");
		return -EINVAL;
	}

	int err = 0;
	if (!live || mainSource == NULL) {
		dTime = time.elapsed();
		demux->demuxOne();
		dTime = time.elapsed() - dTime;
	} else {
		/*
		 * live pipeline may produce a lot of data, so they may needed to
		 * be pulled more often
		 */
		CircularBuffer *buf = mainSource->getCircularBuffer();
		int bCnt = demux->audioBufferCount();
		while (buf->usedSize() > 1024 * 100) {
			int err = demux->demuxOne();
			if (err)
				return err;
		}
		/*
		 * During discontinuties, live pipelines may create hundreds of
		 * buffers, which should be discarded. Otherwise long latencies
		 * are faced.
		 */
		if (demux->audioBufferCount() - bCnt > 100)
			demux->flush();
	}
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
	return 0;
}

void BaseLmmPlayer::audioPopTimerTimeout()
{
#ifdef CONFIG_ALSA
	if (alsaControl)
		alsaControl->mute(false);
#endif
}

void BaseLmmPlayer::streamInfoFound()
{
	foreach (BaseLmmElement *el, elements) {
		if (demux && !live)
			el->setStreamDuration(demux->getTotalDuration());
		else
			el->setStreamDuration(-1);
	}
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
