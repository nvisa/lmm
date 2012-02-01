#include "baselmmdemux.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "emdesk/debug.h"

extern "C" {
	#include "libavformat/avformat.h"
}

static void deletePacket(AVPacket *packet)
{
	if (packet->data != NULL)
		av_free_packet(packet);
	delete packet;
}

BaseLmmDemux::BaseLmmDemux(QObject *parent) :
	BaseLmmElement(parent)
{
	static bool avRegistered = false;
	if (!avRegistered) {
		mDebug("registering all formats");
		av_register_all();
		avRegistered = true;
	}
	audioClock = new StreamTime(this);
	videoClock = new StreamTime(this);
	demuxAudio = demuxVideo = true;
	context = NULL;
	audioStream = NULL;
	videoStream = NULL;
}

qint64 BaseLmmDemux::getCurrentPosition()
{
	return audioClock->getCurrentTime();
}

qint64 BaseLmmDemux::getTotalDuration()
{
	if (audioStream)
		return audioStream->duration * audioTimeBaseN / 1000;
	if (videoStream)
		return videoStream->duration * videoTimeBaseN / 1000;
	return 0;
}

int BaseLmmDemux::findStreamInfo()
{
	int err = av_find_stream_info(context);
	if (err < 0)
		return err;
	mDebug("%d streams, %d programs present in the file", context->nb_streams, context->nb_programs);
	for (unsigned int i = 0; i < context->nb_streams; ++i) {
		mDebug("stream: type %d", context->streams[i]->codec->codec_type);
		if (context->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
			videoStreamIndex = i;
		if (context->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO)
			audioStreamIndex = i;
	}
	if (audioStreamIndex < 0 && videoStreamIndex < 0) {
		mDebug("no compatiple stream found");
		return -EINVAL;
	}

	audioClock->setStartTime(0);
	videoClock->setStartTime(0);
	/* derive necessary information from streams */
	if (audioStreamIndex >= 0) {
		mDebug("audio stream found at index %d", audioStreamIndex);
		audioStream = context->streams[audioStreamIndex];
		AVRational r = audioStream->time_base;
		audioTimeBase = 1000000 * r.num / r.den;
		audioTimeBaseN = qint64(1000000000) * r.num / r.den;
		mDebug("audioBase=%d", audioTimeBase);
	}
	if (videoStreamIndex >= 0) {
		mDebug("video stream found at index %d", videoStreamIndex);
		videoStream = context->streams[videoStreamIndex];
		AVRational r = videoStream->time_base;
		r = videoStream->time_base;
		videoTimeBase = 1000000 * r.num / r.den;
		videoTimeBaseN = qint64(1000000000) * r.num / r.den;
		mDebug("videoBase=%d", videoTimeBase);
	}

	streamPosition = 0;

	return 0;
}

int BaseLmmDemux::setSource(QString filename)
{
	sourceUrlName = filename;
	int err = av_open_input_file(&context, qPrintable(sourceUrlName), NULL, 0, NULL);
	if (err)
		return err;
	return 0;
}

int BaseLmmDemux::demuxOne()
{
	if (!foundStreamInfo) {
		if (findStreamInfo()) {
			mDebug("error in stream info");
		} else {
			foundStreamInfo = true;
			if (debugMessagesAvailable())
				dump_format(context, 0, qPrintable(sourceUrlName), false);
			emit streamInfoFound();
		}
	}
	static AVPacket *packet = NULL;
	if (packet)
		deletePacket(packet);
	packet = nextPacket();
	if (!packet)
		return -ENOENT;
	if (packet->stream_index == audioStreamIndex) {
		mInfo("new audio stream: size=%d", packet->size);
		if (demuxAudio) {
			RawBuffer *buf = new RawBuffer(packet->data, packet->size);
			buf->setDuration(packet->duration * audioTimeBase);
			if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
				buf->setPts(packet->pts * audioTimeBaseN / 1000);
			} else {
				buf->setPts(-1);
			}
			if (packet->dts != int64_t(AV_NOPTS_VALUE)) {
				buf->setDts(packet->dts * audioTimeBaseN / 1000);
			} else {
				buf->setDts(-1);
			}
			audioBuffers << buf;
		}
	} else if (packet->stream_index == videoStreamIndex) {
		mInfo("new video stream: size=%d pts=%lld duration=%d dflags=%d", packet->size,
			   packet->pts == (int64_t)AV_NOPTS_VALUE ? -1 : packet->pts ,
			   packet->duration, packet->flags);
		if (demuxVideo) {
			RawBuffer *buf = new RawBuffer(packet->data, packet->size);
			buf->setDuration(packet->duration * videoTimeBase);
			if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
				buf->setPts(packet->pts * videoTimeBaseN / 1000);
			} else {
				buf->setPts(-1);
			}
			if (packet->dts != int64_t(AV_NOPTS_VALUE)) {
				buf->setDts(packet->dts * videoTimeBaseN / 1000);
			} else {
				buf->setDts(-1);
			}
			videoBuffers << buf;
		}
	}
	return 0;
}

int BaseLmmDemux::flush()
{
	qDeleteAll(videoBuffers);
	qDeleteAll(audioBuffers);
	videoBuffers.clear();
	audioBuffers.clear();
	return BaseLmmElement::flush();
}

StreamTime * BaseLmmDemux::getStreamTime(BaseLmmDemux::streamType stream)
{
	if (stream == STREAM_AUDIO)
		return audioClock;
	if (stream == STREAM_VIDEO)
		return videoClock;
	return audioClock;
}

int BaseLmmDemux::getAudioSampleRate()
{
	if (audioStream)
		return audioStream->codec->sample_rate;
	return 0;
}

RawBuffer * BaseLmmDemux::nextAudioBuffer()
{
	if (audioBuffers.size()) {
		sentBufferCount++;
		return audioBuffers.takeFirst();
	}

	return NULL;
}

RawBuffer * BaseLmmDemux::nextVideoBuffer()
{
	if (videoBuffers.size()) {
		sentBufferCount++;
		return videoBuffers.takeFirst();
	}

	return NULL;
}

int BaseLmmDemux::audioBufferCount()
{
	return audioBuffers.count();
}

int BaseLmmDemux::videoBufferCount()
{
	return videoBuffers.count();
}

int BaseLmmDemux::start()
{
	videoStreamIndex = audioStreamIndex = -1;
	audioStream = videoStream = NULL;
	foundStreamInfo = false;
	return BaseLmmElement::start();
}

int BaseLmmDemux::stop()
{
	av_close_input_file(context);
	context = NULL;
	return BaseLmmElement::stop();
}

int BaseLmmDemux::seekTo(qint64 pos)
{
	int flags = 0;
	if (pos < streamPosition)
		flags = AVSEEK_FLAG_BACKWARD;
	if (pos < 0)
		pos = 0;
	if (pos > streamDuration)
		return -EINVAL;
	if (videoStreamIndex != -1) {
		int err = av_seek_frame(context, videoStreamIndex,
								pos / videoTimeBase, flags);
		if (err < 0) {
			mDebug("error during seek");
			return err;
		}
	} else if (audioStreamIndex != -1) {
		int err = av_seek_frame(context, audioStreamIndex,
								pos / audioTimeBase, flags);
		if (err < 0) {
			mDebug("error during seek");
			return err;
		}
	} else {
		int err = av_seek_frame(context, -1, pos, flags);
		if (err < 0) {
			mDebug("error during seek");
			return err;
		}
	}
	videoBuffers.clear();
	audioBuffers.clear();
	return 0;
}

AVPacket * BaseLmmDemux::nextPacket()
{
	AVPacket *packet = new AVPacket;
	if (av_read_frame(context, packet)) {
		deletePacket(packet);
		return NULL;
	}
	return packet;
}
