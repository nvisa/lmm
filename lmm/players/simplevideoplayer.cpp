#include "simplevideoplayer.h"

#include <lmm/debug.h>
#include <lmm/ffmpeg/baselmmdemux.h>
#include <lmm/ffmpeg/ffmpegdecoder.h>
#include <lmm/baselmmoutput.h>

#include <errno.h>

SimpleVideoPlayer::SimpleVideoPlayer(QObject *parent) :
	BasePlayer(parent)
{
	demuxer = new BaseLmmDemux;
	elements << demuxer;

	decoder = new FFmpegDecoder;
	elements << decoder;

	vout = NULL;
}

qint64 SimpleVideoPlayer::getDuration()
{
	return demuxer->getTotalDuration();
}

qint64 SimpleVideoPlayer::getPosition()
{
	return lastPts;
}

QList<QVariant> SimpleVideoPlayer::extraDebugInfo()
{
	QList<QVariant> list;
	list << waitingEOF;
	list << eof;
	list << waitingDemux;
	return list;
}

void SimpleVideoPlayer::timeoutHandler()
{
	checkEOF();
}

int SimpleVideoPlayer::startPlayer()
{
	waitingDemux = false;
	waitingEOF = false;
	eof = false;
	lastPts = 0;
	waitingEOF = false;
	if (!vout) {
		vout = createVideoOutput();
		if (!vout)
			return -ENOMEM;
		elements << vout;
		startElement(vout);
	}

	finishedThreadCount = 0;
	demuxer->setSource(sourceUrl.path());
	createOpThreadPri(&SimpleVideoPlayer::demux, "SimplePlayerDemuxThread", SimpleVideoPlayer, QThread::LowestPriority);
	createOpThreadPri(&SimpleVideoPlayer::queueForVideoDecode, "SimplePlayerQueueVideoDecodeThread", SimpleVideoPlayer, QThread::LowestPriority);
	createOpThreadPri(&SimpleVideoPlayer::decodeVideo, "SimplePlayerDecodeThread", SimpleVideoPlayer, QThread::LowestPriority);
	createOpThreadPri(&SimpleVideoPlayer::queueForVideoDisplay, "SimplePlayerQueueVideoDisplayThread", SimpleVideoPlayer, QThread::LowestPriority);
	createOpThreadPri(&SimpleVideoPlayer::display, "SimplePlayerDisplayThread", SimpleVideoPlayer, QThread::LowestPriority);
	return 0;
}

int SimpleVideoPlayer::stopPlayer()
{
	mDebug("destroying threads");
	qDeleteAll(threads);
	threads.clear();

	return 0;
}

int SimpleVideoPlayer::demux()
{
	if (demuxer->getOutputBufferCount() > 200) {
		mDebug("demuxed too much, waiting...");
		waitingDemux = true;
		if (demuxer->waitOutputBuffers(0, 100)) {
			mDebug("no demuxer buffers anymore");
			waitingEOF = true;
			waitingDemux = false;
			return -ENOENT;
		}
		waitingDemux = false;
	}
	int err = demuxer->demuxOne();
	if (err) {
		waitingEOF = true;
		mDebug("demuxer finished with err %d, buffer count %d, total %d", err,
			   demuxer->getOutputBufferCount(), demuxer->getSentBufferCount());
		return err;
	}
	return err;
}

int SimpleVideoPlayer::queueForVideoDecode()
{
	RawBuffer buf = demuxer->nextBufferBlocking(0);

	if (buf.size()) {
		if (!decoder->getStream()) {
			decoder->setStream(demuxer->getVideoCodecContext());
		}
		return decoder->addBuffer(0, buf);
	}

	return -ENOENT;
}

int SimpleVideoPlayer::decodeVideo()
{
	int err = decoder->processBlocking();
	if (err) {
		mDebug("decoder error %d, exiting", err);
		if (err == -1)
			return 0;
		return err;
	}
	return 0;
}

int SimpleVideoPlayer::queueForVideoDisplay()
{
	RawBuffer buf = decoder->nextBufferBlocking(0);
	if (buf.size()) {
		mInfo("display %d", buf.size());
		return vout->addBuffer(0, buf);
	}

	return -ENOENT;
}

int SimpleVideoPlayer::display()
{
	int err = vout->processBlocking();
	lastPts = vout->getLastOutputPts();
	return err;
}

void SimpleVideoPlayer::checkEOF()
{
	if (waitingEOF) {
#if 0
		int cnt = 0;
		foreach (BaseLmmElement *el, elements)
			cnt += el->getInputSemCount(0);
		if (!cnt) {
			waitingEOF = false;
			eof = true;
			lastPts = demuxer->getTotalDuration();
			emit playbackFinished(0);
		}
#else
#endif
	}
}

void SimpleVideoPlayer::threadFinished(LmmThread *)
{
	if (++finishedThreadCount == threads.size()) {
		eof = true;
		lastPts = demuxer->getTotalDuration();
		emit playbackFinished(0);
	}
}
