#define __STDC_CONSTANT_MACROS

#include "avidemux.h"
#include "rawbuffer.h"
#define DEBUG
#include "debug.h"

extern "C" {
	#define __STDC_CONSTANT_MACROS
	#include <stdint.h>
	#include "libavformat/avformat.h"
}

AviDemux::AviDemux(QObject *parent) :
	BaseLmmDemux(parent)
{
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

int AviDemux::demuxAll()
{
#if 0
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
#endif
	return 0;
}

