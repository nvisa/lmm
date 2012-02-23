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
#include <QSemaphore>

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

	streamTime = new StreamTime(this);
	live = false;
	timer = new QTimer(this);
	timer->setSingleShot(true);
	connect(timer, SIGNAL(timeout()), this, SLOT(decodeLoop()));

	waitProducer = new QSemaphore;
	waitConsumer = new QSemaphore;
	videoThreadWatcher = new QFutureWatcher<int>;
	QThreadPool::globalInstance()->reserveThread();

	noAudio = noVideo = false;
}

BaseLmmPlayer::~BaseLmmPlayer()
{
	QThreadPool::globalInstance()->releaseThread();
	qDeleteAll(elements);
	elements.clear();
}

int BaseLmmPlayer::play(QString url)
{
	if (state != STOPPED)
		return -EBUSY;

	mInfo("starting playback");

	connect(demux, SIGNAL(streamInfoFound()), SLOT(streamInfoFound()));
	if (!url.isEmpty()) {
		int err = demux->setSource(url);
		if (err)
			return err;
	}

	streamTime->start();
	foreach (BaseLmmElement *el, elements) {
		mInfo("starting element %s", el->metaObject()->className());
		el->setStreamTime(streamTime);
		el->flush();
		el->start();
	}

	mInfo("all elements started, game is a foot");
	timer->start(10);
	state = RUNNING;

	return 0;
}

int BaseLmmPlayer::stop()
{
	if (state != RUNNING)
		return -EBUSY;

	disconnect(demux, SIGNAL(streamInfoFound()));
	foreach (BaseLmmElement *el, elements) {
		el->setStreamTime(NULL);
		el->stop();
	}
	streamTime->stop();
	state = STOPPED;

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

int BaseLmmPlayer::wait(int timeout)
{
	waitConsumer->release(1);
	/* TODO: Thread handling */
	if (waitProducer->tryAcquire(1, timeout))
		return 0;
	return -ETIMEDOUT;
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
	if (pos > demux->getTotalDuration()) {
		stop();
		emit finished();
		return -EINVAL;
	}
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

int BaseLmmPlayer::flush()
{
	foreach (BaseLmmElement *el, elements)
		el->flush();
	return 0;
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

void BaseLmmPlayer::useNoAudio()
{
	noAudio = true;
}

void BaseLmmPlayer::useNoVideo()
{
	noVideo = true;
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
		mInfo("demuxing next packet");
		dTime = time.elapsed();
		err = demux->demuxOne();
		dTime = time.elapsed() - dTime;
	} else {
		/*
		 * live pipeline may produce a lot of data, so they may needed to
		 * be pulled more often
		 */
		CircularBuffer *buf = mainSource->getCircularBuffer();
		int bCnt = demux->audioBufferCount();
		while (buf->usedSize() > 1024 * 100) {
			mInfo("demuxing next packet");
			int err = demux->demuxOne();
			if (err)
				break;
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
			mInfo("decoding audio");
			audioLoop();
			mInfo("decoded audio");
			aTime = time.elapsed() - aTime;

			if (!noAudio) {
				if (videoOutput)
					videoOutput->setOutputDelay(audioOutput->getLatency());
			} else
				videoOutput->setOutputDelay(0);
		} else
			demux->setAudioDemuxing(false);
		vTime = time.elapsed();
		mInfo("decoding video");
		if (videoDecoder)
			videoLoop();
		else
			demux->setVideoDemuxing(false);
		vTime = time.elapsed() - vTime;
	} else if (err == -ENOENT) {
		mDebug("decoding finished");
		stop();
		emit finished();
		return 0;
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
	int rate = demux->getAudioSampleRate();
	/* default audio rate is 48 khz, change if needed */
	if (rate != 48000 && audioOutput)
		audioOutput->setParameter("audioRate", rate);
}

void BaseLmmPlayer::audioLoop()
{
	RawBuffer *buf = demux->nextAudioBuffer();
	if (noAudio) {
		if (buf)
			delete buf;
		return;
	}
	if (buf)
		audioDecoder->addBuffer(buf);
	if (audioDecoder->decode())
		mDebug("audio decoder reported error");
	buf = audioDecoder->nextBuffer();
	if (buf) {
		audioOutput->addBuffer(buf);
		/*
		 * If we are not a live pipeline, sync current time
		 * with audio timestamp. In live pipelines, most of the
		 * time there are other clock providers which explicitly
		 * syncs stream time with their internal clock
		 */
		if (!live)
			streamTime->setCurrentTime(buf->getPts());
	}
	if (!audioOutput->output()) {
		if (waitConsumer->tryAcquire(1))
			waitProducer->release(1);
	}
}

void BaseLmmPlayer::videoLoop()
{
	if (!videoThreadWatcher->future().isFinished()) {
		mDebug("previous decode operation is not finished");
		return;
	}
	RawBuffer *buf = demux->nextVideoBuffer();
	if (noVideo) {
		if (buf)
			delete buf;
		return;
	}
	if (buf)
		videoDecoder->addBuffer(buf);
	videoThreadWatcher->setFuture(QtConcurrent::run(videoDecoder, &BaseLmmDecoder::decode));
	buf = videoDecoder->nextBuffer();
	if (videoOutput) {
		if (buf)
			videoOutput->addBuffer(buf);
		if (!videoOutput->output()) {
			if (waitConsumer->tryAcquire())
				waitProducer->release(1);
		}
	} else {
		if (buf)
			delete buf;
		if (waitConsumer->tryAcquire())
			waitProducer->release(1);
	}
}
