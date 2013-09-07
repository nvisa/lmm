#define __STDC_CONSTANT_MACROS

#include "audioencoder.h"
#include "debug.h"

#include <errno.h>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h> /* for URLContext on x86 */
	#include "ffcompat.h"
}

AudioEncoder::AudioEncoder(QObject *parent) :
	BaseLmmElement(parent)
{
	outbuf = new uchar[FF_MIN_BUFFER_SIZE];
	outSize = FF_MIN_BUFFER_SIZE;
	codec = NULL;
	c = NULL;
}

int AudioEncoder::start()
{
	int err = startCodec();
	if (err)
		return err;
	return BaseLmmElement::start();
}

int AudioEncoder::stop()
{
	stopCodec();
	return BaseLmmElement::stop();
}

int AudioEncoder::startCodec()
{
	if (codec) {
		avcodec_close(c);
		av_free(c);
		codec = NULL;
		c = NULL;
	}
	codec = avcodec_find_encoder(CODEC_ID_MP2);
	if (!codec) {
		mDebug("cannot find codec");
		return -ENOENT;
	}
	c = avcodec_alloc_context();
	c->bit_rate = 64000;
	c->sample_rate = 44100;
	c->channels = 2;
	c->sample_fmt = AV_SAMPLE_FMT_S16;

	if (avcodec_open(c, codec) < 0) {
		mDebug("cannot open codec");
		return -EINVAL;
	}
	avail = 0;
	sbuf = RawBuffer("unknown", c->frame_size * c->channels * sizeof(short));
	short *samples = (short *)sbuf.data();
	float t = 0;
	float tincr = 2 * M_PI * 440.0 / c->sample_rate;
	for (int i = 0; i < c->frame_size; i++) {
		samples[2 * i] = (int)(sin(t) * 10000);
		samples[2 * i + 1] = samples[2 * i];
		t += tincr;
	}

	return 0;
}

int AudioEncoder::stopCodec()
{
	return 0;
}

int AudioEncoder::processBuffer(RawBuffer buf)
{
	int insize = sbuf.size();
	short *samples = (short *)sbuf.data();
	if (buf.size() <= insize - avail) {
		avail += buf.size();
		if (avail == insize) {
			encode(samples);
			avail = 0;
		}
	} else {
		encode(samples);
		avail = insize - avail;
	}
	return 0;
}

int AudioEncoder::encode(const short *samples)
{
	mInfo("encoding new audio buffer");
	int size = avcodec_encode_audio(c, outbuf, outSize, samples);
	if (size) {
		RawBuffer buf("audio/mpeg2", outbuf, size);
		return newOutputBuffer(0, buf);
	}
	return 0;
}
