#include "avidecoder.h"
#include "avidemux.h"
#include "mad.h"
#include "rawbuffer.h"
#include "alsaoutput.h"
#include "fboutput.h"
#include "dmaidecoder.h"
#include "streamtime.h"
#include "emdesk/debug.h"
#include "emdesk/hardwareoperations.h"
#include "alsa/alsa.h"

#include <QTime>
#include <QTimer>
#include <QThread>

#include <errno.h>

AviDecoder::AviDecoder(QObject *parent) :
	QObject(parent)
{
	streamTime = new StreamTime;
	audioDecoder = new Mad;
	videoDecoder = new DmaiDecoder;
	audioOutput = new AlsaOutput;
	videoOutput = new FbOutput;
	demux = new AviDemux;
	elements << demux;
	elements << videoDecoder;
	elements << videoOutput;
	elements << audioDecoder;
	elements << audioOutput;
	state = STOPPED;
}

AviDecoder::~AviDecoder()
{
	if (videoDecoder) {
		delete videoDecoder;
		delete audioDecoder;
		delete videoDecoder;
		delete audioOutput;
		delete videoOutput;
		delete demux;
		videoDecoder = NULL;
		elements.clear();
	}
}

int AviDecoder::startDecoding()
{
	if (state == STOPPED) {
		state = RUNNING;
		int err = demux->setSource("/media/net/Fringe.S04E06.HDTV.XviD-LOL.[VTV].avi");
		if (err)
			return err;
		connect(demux, SIGNAL(newAudioFrame()), SLOT(newAudioFrame()), Qt::QueuedConnection);
		HardwareOperations::blendOSD(true, 31);

		foreach (BaseLmmElement *el, elements) {
			el->start();
			el->setStreamDuration(demux->getTotalDuration());
			el->setStreamTime(streamTime);
		}

		timer = new QTimer(this);
		timer->setSingleShot(true);
		connect(timer, SIGNAL(timeout()), this, SLOT(decodeLoop()));
		timer->start(10);
	}

	return 0;
}

void AviDecoder::stopDecoding()
{
	if (state == RUNNING) {
		state = STOPPED;
		foreach (BaseLmmElement *el, elements)
			el->stop();
		HardwareOperations::blendOSD(false);
	}
}

qint64 AviDecoder::getDuration()
{
	return demux->getTotalDuration();
}

qint64 AviDecoder::getPosition()
{
	return streamTime->getCurrentTime();
}

int AviDecoder::seekTo(qint64 pos)
{
	audioOutput->alsaControl()->mute(true);
	if (!demux->seekTo(pos)) {
		mDebug("seek ok");
		foreach (BaseLmmElement *el, elements)
			el->flush();
	}
	QTimer::singleShot(200, this, SLOT(audioPopTimerTimeout()));
	return 0;
}

int AviDecoder::seek(qint64 pos)
{
	return seekTo(demux->getCurrentPosition() + pos);
}

void AviDecoder::setMute(bool mute)
{
	audioOutput->alsaControl()->mute(mute);
}

void AviDecoder::setVolumeLevel(int per)
{
	audioOutput->alsaControl()->setCurrentVolumeLevel(per);
}

int AviDecoder::getVolumeLevel()
{
	return audioOutput->alsaControl()->currentVolumeLevel();
}

void AviDecoder::newAudioFrame()
{
}

void AviDecoder::decodeLoop()
{
	if (state != RUNNING) {
		mDebug("we are not in a running state");
		return;
	}
	int err = demux->demuxOne();
	if (!err) {
		streamTime->setCurrentTime(demux->getCurrentPosition());
		audioLoop();
		videoOutput->setOutputDelay(audioOutput->getLatency());
		videoLoop();
	} else if (err == -ENOENT) {
		mDebug("decoding finished");
		stopDecoding();
	}
	timer->start(5);
}

void AviDecoder::audioPopTimerTimeout()
{
	audioOutput->alsaControl()->mute(false);
}

void AviDecoder::audioLoop()
{
	RawBuffer *buf = demux->nextAudioBuffer();
	if (buf)
		audioDecoder->addBuffer(buf);
	if (audioDecoder->decodeAll())
		mDebug("audio decoder reported error");
	buf = audioDecoder->nextBuffer();
	if (buf)
		audioOutput->addBuffer(buf);
	audioOutput->output();
}

void AviDecoder::videoLoop()
{
	RawBuffer *buf = demux->nextVideoBuffer();
	if (buf)
		videoDecoder->addBuffer(buf);
	videoDecoder->decodeOne();
	buf = videoDecoder->nextBuffer();
	if (buf)
		videoOutput->addBuffer(buf);
	videoOutput->output();
}
