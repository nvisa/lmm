#define __STDC_CONSTANT_MACROS

#include "mpegtsmux.h"
#include "debug.h"
#include "streamtime.h"

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h> /* for URLContext on x86 */
	#include "ffcompat.h"
}

MpegTsMux::MpegTsMux(QObject *parent) :
	BaseLmmMux(parent)
{
	sourceUrlName = "lmmmuxo";
	fmt = av_guess_format("mpegts", NULL, NULL);
	addNewInputChannel();
}

QString MpegTsMux::mimeType()
{
	return "video/mpegts";
}

int MpegTsMux::findInputStreamInfo()
{
	/* since we know the input format no need for long probing */
	mDebug("using static h.264 mux information for fast mux initialization");
	return 0;
}

int MpegTsMux::initMuxer()
{
	int w = 0;//getParameter("videoWidth").toInt();
	int h = 0;//getParameter("videoHeight").toInt();
	if (!w)
		w = 720;
	if (!h)
		h = 576;
	/*
	 * We know input codec information so we don't use
	 * long lasting av_find_stream_info() function, instead
	 * we fill codec information by hand. We use printInputInfo()
	 * function for finding necessary values
	 */
	AVCodecContext *codec;
	uint64_t extra_size;

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
	context->max_delay = 0;//0.7 * AV_TIME_BASE; //from ffmpeg source
	context->loop_output = 0;
	context->flags |= AVFMT_FLAG_NONBLOCK;

	if (audioCtx)
		addAudioStream();

	if (!(fmt->flags & AVFMT_NOFILE) && avio_open(&context->pb, qPrintable(sourceUrlName), URL_WRONLY) < 0) {
		mDebug("error opening stream file '%s'", qPrintable(sourceUrlName));
		err = -EACCES;
		goto err_out1;
	}
	av_write_header(context);
	mInfo("output header written");

	foundStreamInfo = true;
	return 0;

err_out1:
	avformat_free_context(context);
	return err;
}

qint64 MpegTsMux::packetTimestamp(int stream)
{
	if (!context->nb_streams)
		return 0;
	if (!streamTime)
		return 0;
	static int spv = -1;
	static int spa = 0;
	static qint64 lapts = 0;
	if (stream == 0) {
		spv = muxedBufferCount[0];
		return 90000ll * muxedBufferCount[0] / 25.0;
	}
	qint64 pts = 90000ll * muxedBufferCount[0] / 25.0;
	if (pts > lapts) {
		spa = 0;
		lapts = pts;
		return pts;
	}

	lapts = pts + 90000 * (++spa) / 17;
	return lapts;
	if (spv < muxedBufferCount[0]) {
		spa = 0;//muxedBufferCount[1];
		return 0;
	} else {
		qDebug() << "here";
		return (90000ll * spv / 25.0) + 90000 * (++spa) / 17;
	}
	return 90000ll * muxedBufferCount[1] / 16;//* 1152 * 2 / 44100;
}

int MpegTsMux::addAudioStream()
{
	AVStream *st = av_new_stream(context, 0);
	if (!st) {
		mDebug("cannot create audio stream");
		return -ENOENT;
	}
	st->codec = audioCtx;
	st->stream_copy = 1;
	st->pts.num = st->time_base.num = st->codec->time_base.num;
	st->pts.den = st->time_base.den = st->codec->time_base.den;
	return 0;
}
