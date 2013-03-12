#define __STDC_CONSTANT_MACROS

#include "baselmmdemux.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "debug.h"

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
	demuxAudio = demuxVideo = true;
	context = NULL;
	audioStream = NULL;
	videoStream = NULL;
	libavAnalayzeDuration = 5000000; /* this is ffmpeg default */
}

qint64 BaseLmmDemux::getCurrentPosition()
{
	return streamTime->getCurrentTime();
}

qint64 BaseLmmDemux::getTotalDuration()
{
	if (videoStream)
		return videoStream->duration * videoTimeBaseN / 1000;
	if (audioStream)
		return audioStream->duration * audioTimeBaseN / 1000;
	return 0;
}

int BaseLmmDemux::findStreamInfo()
{
	context->max_analyze_duration = libavAnalayzeDuration;
	int err = av_find_stream_info(context);
	if (err < 0)
		return err;
	mDebug("%d streams, %d programs present in the file", context->nb_streams, context->nb_programs);
	for (unsigned int i = 0; i < context->nb_streams; ++i) {
		mDebug("stream: type %d", context->streams[i]->codec->codec_type);
		if (context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
			videoStreamIndex = i;
		if (context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			if (context->streams[i]->codec->channels != 0
					|| context->streams[i]->codec->sample_rate != 0)
				audioStreamIndex = i;
		}
	}
	if (audioStreamIndex < 0 && videoStreamIndex < 0) {
		mDebug("no compatiple stream found");
		return -EINVAL;
	}

	/* derive necessary information from streams */
	if (audioStreamIndex >= 0) {
		mDebug("audio stream found at index %d", audioStreamIndex);
		audioStream = context->streams[audioStreamIndex];
		AVRational r = audioStream->time_base;
		audioTimeBaseN = qint64(1000000000) * r.num / r.den;
		mDebug("audioBase=%lld", audioTimeBaseN);
	}
	if (videoStreamIndex >= 0) {
		mDebug("video stream found at index %d", videoStreamIndex);
		videoStream = context->streams[videoStreamIndex];
		AVRational r = videoStream->time_base;
		r = videoStream->time_base;
		videoTimeBaseN = qint64(1000000000) * r.num / r.den;
		mDebug("videoBase=%lld", videoTimeBaseN);
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
			RawBuffer buf("audio/x-raw-int", packet->data, packet->size);
			buf.setDuration(packet->duration * audioTimeBaseN / 1000);
			if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
				buf.setPts(packet->pts * audioTimeBaseN / 1000);
			} else {
				buf.setPts(-1);
			}
			if (packet->dts != int64_t(AV_NOPTS_VALUE)) {
				buf.setDts(packet->dts * audioTimeBaseN / 1000);
			} else {
				buf.setDts(-1);
			}
			audioBuffers << buf;
		}
	} else if (packet->stream_index == videoStreamIndex) {
		mInfo("new video stream: size=%d pts=%lld duration=%d dflags=%d", packet->size,
			   packet->pts == (int64_t)AV_NOPTS_VALUE ? -1 : packet->pts ,
			   packet->duration, packet->flags);
		if (demuxVideo) {
			RawBuffer buf("video/x-raw-yuv", packet->data, packet->size);
			buf.setDuration(packet->duration * videoTimeBaseN / 1000);
			if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
				buf.setPts(packet->pts * videoTimeBaseN / 1000);
			} else {
				buf.setPts(-1);
			}
			if (packet->dts != int64_t(AV_NOPTS_VALUE)) {
				buf.setDts(packet->dts * videoTimeBaseN / 1000);
			} else {
				buf.setDts(-1);
			}
			videoBuffers << buf;
		}
	}
	return 0;
}

int BaseLmmDemux::flush()
{
	videoBuffers.clear();
	audioBuffers.clear();
	return BaseLmmElement::flush();
}

int BaseLmmDemux::getAudioSampleRate()
{
	if (audioStream)
		return audioStream->codec->sample_rate;
	return 0;
}

RawBuffer BaseLmmDemux::nextAudioBuffer()
{
	if (audioBuffers.size()) {
		sentBufferCount++;
		return audioBuffers.takeFirst();
	}

	return RawBuffer();
}

RawBuffer BaseLmmDemux::nextVideoBuffer()
{
	if (videoBuffers.size()) {
		sentBufferCount++;
		return videoBuffers.takeFirst();
	}

	return RawBuffer();
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
	streamPosition = 0;
	sourceUrlName = "";
	videoStreamIndex = audioStreamIndex = -1;
	audioStream = videoStream = NULL;
	foundStreamInfo = false;
	return BaseLmmElement::start();
}

int BaseLmmDemux::stop()
{
	if (context) {
		av_close_input_file(context);
		context = NULL;
	}
	return BaseLmmElement::stop();
}

int BaseLmmDemux::seekTo(qint64 pos)
{
	int flags = 0;
	if (pos < streamTime->getCurrentTime())
		flags = AVSEEK_FLAG_BACKWARD;
	if (pos < 0)
		pos = 0;
	if (pos > streamDuration)
		return -EINVAL;
	if (videoStreamIndex != -1) {
		int err = av_seek_frame(context, videoStreamIndex,
								pos * 1000 / videoTimeBaseN, flags);
		if (err < 0) {
			mDebug("error during seek");
			return err;
		}
	} else if (audioStreamIndex != -1) {
		int err = av_seek_frame(context, audioStreamIndex,
								pos * 1000 / audioTimeBaseN, flags);
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
