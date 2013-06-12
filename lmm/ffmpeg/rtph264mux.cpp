#define __STDC_CONSTANT_MACROS

#include "rtph264mux.h"

#include "debug.h"

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavformat/rtpenc.h>
	#ifdef CodecType
	#undef CodecType
	#endif
}

RtpH264Mux::RtpH264Mux(QObject *parent) :
	RtpMux(parent)
{
}

Lmm::CodecType RtpH264Mux::codecType()
{
	return Lmm::CODEC_H264;
}

int RtpH264Mux::initMuxer()
{
	int w = getParameter("videoWidth").toInt();
	int h = getParameter("videoHeight").toInt();
	if (!w)
		w = 1280;
	if (!h)
		h = 720;
	/*
	 * We know input codec information so we don't use
	 * long lasting av_find_stream_info() function, instead
	 * we fill codec information by hand. We use printInputInfo()
	 * function for finding necessary values
	 */
	AVCodecContext *codec;
	uint64_t extra_size;
	RTPMuxContext *rtpCtx;

	int err = 0;
	if (!fmt)
		fmt = av_guess_format(NULL, qPrintable(sourceUrlName), NULL);
	if (!fmt) {
		mDebug("cannot guess codec format");
		return -EINVAL;
	}
	mInfo("format found for output: %s %s", fmt->long_name, fmt->mime_type);

	context = avformat_alloc_context();
	if (!context)
		return -ENOMEM;
	snprintf(context->filename, sizeof(context->filename), "%s", qPrintable(sourceUrlName));
	context->oformat = fmt;
	mInfo("allocated output context");

	/* Let's add video stream */
	AVStream *st = av_new_stream(context, 0);
	if (!st) {
		mDebug("cannot create video stream");
		err = -ENOENT;
		goto err_out1;
	}
	mInfo("created new video stream");

	codec = st->codec;
	codec->codec_id = CODEC_ID_H264;
	codec->codec_type = AVMEDIA_TYPE_VIDEO;
	codec->bit_rate = 0;
	codec->rc_max_rate = 0;
	codec->rc_buffer_size = 0;
	extra_size = (uint64_t)22 + FF_INPUT_BUFFER_PADDING_SIZE;
	//TODO: we need to free codec extra data
	codec->extradata = (uint8_t *)av_mallocz(extra_size);

	codec->time_base = st->time_base;
	codec->pix_fmt = (PixelFormat)0;
	codec->width = w;
	codec->height = h;
	codec->has_b_frames = 0; //TODO: rtp does not support b-frames ???
	codec->sample_aspect_ratio = st->sample_aspect_ratio = av_d2q(0, 255); //ffmpeg does so
	st->stream_copy = 1;
	st->pts.num = codec->time_base.num;
	st->pts.den = codec->time_base.den;
	if (context->oformat->flags & AVFMT_GLOBALHEADER)
		codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	mInfo("output codec parameters adjusted");

	context->timestamp = 0;

	context->preload = 0.5 * AV_TIME_BASE; //from ffmpeg source
	context->max_delay = 0.7 * AV_TIME_BASE; //from ffmpeg source
	context->loop_output = 0;
	context->flags |= AVFMT_FLAG_NONBLOCK;

	if (!(fmt->flags & AVFMT_NOFILE) && avio_open(&context->pb, qPrintable(sourceUrlName), URL_WRONLY) < 0) {
		mDebug("error opening stream file '%s'", qPrintable(sourceUrlName));
		err = -EACCES;
		goto err_out1;
	}
	av_write_header(context);
	mInfo("output header written");

	context->flags = AVFMT_FLAG_RTP_HINT;
	/*
	 * ssrc is updated in rtp_write_header so at this point
	 * it is safe to read it
	 */
	mDebug("changing RTP private information");
	rtpCtx = (RTPMuxContext *)context->priv_data;
	rtpCtx->seq = 6666;
	rtpCtx->base_timestamp = 1578998879;
	rtpCtx->ssrc = 0x6335D514;
	mDebug("changed RTP private information");
	emit sdpReady(getSdp());
	return 0;

err_out1:
	avformat_free_context(context);
	return err;
}

int RtpH264Mux::findInputStreamInfo()
{
	/* since we know the input format no need for long probing */
	mDebug("using static h.264 mux information for fast mux initialization");
	return 0;
}
