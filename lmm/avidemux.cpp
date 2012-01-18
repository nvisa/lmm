#include "avidemux.h"
#include "rawbuffer.h"
#define DEBUG
#include "emdesk/debug.h"

extern "C" {
	#include "libavformat/avformat.h"
	#include "libavcodec/avcodec.h"
}

AviDemux::AviDemux(QObject *parent) :
	BaseLmmElement(parent)
{
	av_register_all();
}

#if 0
	AVCodecContext *codecContext;
	AVCodec *codec;
	codecContext = context->streams[audioStream]->codec;
	codec = avcodec_find_decoder(codecContext->codec_id);
	if (codec)
		return -ENOENT;
	mDebug("found audio codec");
	err = avcodec_open(codecContext, codec);
	if (err < 0)
		return err;
#endif
int AviDemux::setSource(QString filename)
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
	if (audioStreamIndex < 0 || videoStreamIndex < 0)
		return -EINVAL;
	mDebug("streams found");
	audioStream = context->streams[audioStreamIndex];
	videoStream = context->streams[videoStreamIndex];

	/* derive necessary information */
	AVRational r = audioStream->time_base;
	audioTimeBase = 1000000 * r.num / r.den;
	r = videoStream->time_base;
	videoTimeBase = 1000000 * r.num / r.den;
	mDebug("audioBase=%d videoBase=%d", audioTimeBase, videoTimeBase);
	streamPosition = 0;
	if (debugMessagesAvailable())
		dump_format(context, 0, qPrintable(filename), false);
	/*qDebug() << "duration" << aStream->duration;
	qDebug() << "nb_frames" << aStream->nb_frames;
	qDebug() << "codec_info_nb_frames" << aStream->codec_info_nb_frames;
	qDebug() << "time_base" << r.num << r.den;*/
	return 0;
}

static void deletePacket(AVPacket *packet)
{
	if (packet->data != NULL)
		av_free_packet(packet);
	delete packet;
}

int AviDemux::demuxOne()
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

int AviDemux::demuxAll()
{
	AVPacket *packet = nextPacket();
	while (packet) {
		if (packet->stream_index == audioStreamIndex) {
			mDebug("new audio stream: size=%d", packet->size);
			RawBuffer *buf = new RawBuffer(packet->data, packet->size);
			buf->setDuration(packet->duration * audioTimeBase);
			audioBuffers << buf;
			streamPosition += buf->getDuration();
			emit newAudioFrame();
		}
		deletePacket(packet);
		packet = nextPacket();
	}
	return 0;
}

qint64 AviDemux::getTotalDuration()
{
	if (audioStream)
		return audioStream->duration * audioTimeBase;
	if (videoStream)
		return videoStream->duration * videoTimeBase;
	return 0;
}

qint64 AviDemux::getCurrentPosition()
{
	return streamPosition;
}

RawBuffer * AviDemux::nextAudioBuffer()
{
	if (audioBuffers.size())
		return audioBuffers.takeFirst();
	outputBufferCount++;
	return NULL;
}

RawBuffer * AviDemux::nextVideoBuffer()
{
	if (videoBuffers.size())
		return videoBuffers.takeFirst();
	outputBufferCount++;
	return NULL;
}

int AviDemux::audioBufferCount()
{
	return audioBuffers.count();
}

int AviDemux::start()
{
	return 0;
}

int AviDemux::stop()
{
	av_close_input_file(context);
	context = NULL;
	return 0;
}

int AviDemux::seekTo(qint64 pos)
{
	int flags = 0;
	if (pos < streamPosition)
		flags = AVSEEK_FLAG_BACKWARD;
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

AVPacket * AviDemux::nextPacket()
{
	AVPacket *packet = new AVPacket;
	if (av_read_frame(context, packet)) {
		deletePacket(packet);
		return NULL;
	}
	return packet;
}
