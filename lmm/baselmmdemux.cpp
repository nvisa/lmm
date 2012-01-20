#include "baselmmdemux.h"
#include "rawbuffer.h"
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
		av_register_all();
		avRegistered = true;
	}
}

qint64 BaseLmmDemux::getCurrentPosition()
{
	return streamPosition;
}

qint64 BaseLmmDemux::getTotalDuration()
{
	if (audioStream)
		return audioStream->duration * audioTimeBase;
	if (videoStream)
		return videoStream->duration * videoTimeBase;
	return 0;
}

int BaseLmmDemux::setSource(QString filename)
{
	if (av_open_input_file(&context, qPrintable(filename), NULL, 0, NULL))
		return -ENOENT;
	int err = av_find_stream_info(context);
	if (err < 0)
		return err;
	videoStreamIndex = audioStreamIndex = -1;
	audioStream = videoStream = NULL;
	for (unsigned int i = 0; i < context->nb_streams; ++i) {
		if (context->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
			videoStreamIndex = i;
		if (context->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO)
			audioStreamIndex = i;
	}
	if (audioStreamIndex < 0 && videoStreamIndex < 0)
		return -EINVAL;

	/* derive necessary information from streams */
	if (audioStreamIndex >= 0) {
		mDebug("audio stream found at index %d", audioStreamIndex);
		audioStream = context->streams[audioStreamIndex];
		AVRational r = audioStream->time_base;
		audioTimeBase = 1000000 * r.num / r.den;
		mDebug("audioBase=%d videoBase=%d", audioTimeBase, videoTimeBase);
	}
	if (videoStreamIndex >= 0) {
		mDebug("video stream found at index %d", videoStreamIndex);
		videoStream = context->streams[videoStreamIndex];
		AVRational r = audioStream->time_base;
		r = videoStream->time_base;
		videoTimeBase = 1000000 * r.num / r.den;
	}

	streamPosition = 0;
	if (debugMessagesAvailable())
		dump_format(context, 0, qPrintable(filename), false);

	return 0;
}

int BaseLmmDemux::demuxOne()
{
	static AVPacket *packet = NULL;
	if (packet)
		deletePacket(packet);
	packet = nextPacket();
	if (!packet)
		return -ENOENT;
	if (packet->stream_index == audioStreamIndex) {
		mInfo("new audio stream: size=%d", packet->size);
		RawBuffer *buf = new RawBuffer(packet->data, packet->size);
		buf->setDuration(packet->duration * audioTimeBase);
		if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
			buf->setPts(packet->pts * audioTimeBase);
			streamPosition = buf->getPts();
		} else {
			buf->setPts(-1);
			streamPosition += buf->getDuration();
		}
		audioBuffers << buf;
	} else if (packet->stream_index == videoStreamIndex) {
		mInfo("new video stream: size=%d pts=%lld duration=%d dflags=%d", packet->size,
			   packet->pts == (int64_t)AV_NOPTS_VALUE ? -1 : packet->pts ,
			   packet->duration, packet->flags);
		RawBuffer *buf = new RawBuffer(packet->data, packet->size);
		buf->setDuration(packet->duration * videoTimeBase);
		if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
			buf->setPts(packet->pts * videoTimeBase);
			if (audioStreamIndex < 0)
				streamPosition = buf->getPts();
		} else {
			buf->setPts(-1);
			if (audioStreamIndex < 0)
				streamPosition += buf->getDuration();
		}
		videoBuffers << buf;
	}
	return 0;
}

RawBuffer * BaseLmmDemux::nextAudioBuffer()
{
	if (audioBuffers.size())
		return audioBuffers.takeFirst();
	sentBufferCount++;
	return NULL;
}

RawBuffer * BaseLmmDemux::nextVideoBuffer()
{
	if (videoBuffers.size())
		return videoBuffers.takeFirst();
	sentBufferCount++;
	return NULL;
}

int BaseLmmDemux::audioBufferCount()
{
	return audioBuffers.count();
}

int BaseLmmDemux::start()
{
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
