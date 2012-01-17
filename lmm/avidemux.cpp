#include "avidemux.h"
#include "rawbuffer.h"
//#define DEBUG
#include "emdesk/debug.h"

extern "C" {
	#include "libavformat/avformat.h"
	#include "libavcodec/avcodec.h"
}

AviDemux::AviDemux(QObject *parent) :
	QObject(parent)
{
	//av_register_input_format(&avi_demuxer);
	av_register_all();
}

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
	audioStream = context->streams[audioStreamIndex];
	videoStream = context->streams[videoStreamIndex];

	/* derive necessary information */
	AVRational r = audioStream->time_base;
	audioFrameDuration = 1000000 * r.num / r.den;
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

void AviDemux::demuxOne()
{
	static AVPacket *packet = NULL;
	if (packet)
		deletePacket(packet);
	packet = nextPacket();
	if (!packet)
		return;
	if (packet->stream_index == audioStreamIndex) {
		mDebug("new audio stream: size=%d", packet->size);
		RawBuffer *buf = new RawBuffer(packet->data, packet->size);
		buf->setDuration(packet->duration * audioFrameDuration);
		audioBuffers << buf;
		streamPosition += buf->getDuration();
	} else if (packet->stream_index == videoStreamIndex) {
		mDebug("new video stream: size=%d", packet->size);
		RawBuffer *buf = new RawBuffer(packet->data, packet->size);
		/* TODO: buffer duration ??? */
		videoBuffers << buf;
	}
}

void AviDemux::demuxAll()
{
	AVPacket *packet = nextPacket();
	while (packet) {
		if (packet->stream_index == audioStreamIndex) {
			mDebug("new audio stream: size=%d", packet->size);
			RawBuffer *buf = new RawBuffer(packet->data, packet->size);
			buf->setDuration(packet->duration * audioFrameDuration);
			audioBuffers << buf;
			streamPosition += buf->getDuration();
			emit newAudioFrame();
		}
		deletePacket(packet);
		packet = nextPacket();
	}
}

unsigned int AviDemux::getTotalDuration()
{
	if (!audioStream)
		return -1;

	return audioStream->duration * audioFrameDuration;
}

int AviDemux::getCurrentPosition()
{
	return streamPosition;
}

RawBuffer * AviDemux::nextAudioBuffer()
{
	if (audioBuffers.size())
		return audioBuffers.takeFirst();
	return NULL;
}

RawBuffer * AviDemux::nextVideoBuffer()
{
	if (videoBuffers.size())
		return videoBuffers.takeFirst();
	return NULL;
}

int AviDemux::audioBufferCount()
{
	return audioBuffers.count();
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
