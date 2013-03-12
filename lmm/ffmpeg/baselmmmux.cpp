#define __STDC_CONSTANT_MACROS

#include "baselmmmux.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "debug.h"

#include <errno.h>

extern "C" {
	#include "libavformat/avformat.h"
	#include "libavformat/avio.h" /* for URLContext on x86 */
}

/* TODO: Fix single instance BaseLmmMux */
//static BaseLmmMux *muxPriv = NULL;
static QList<BaseLmmMux *> muxPriv;

static int lmmUrlOpen(URLContext *h, const char *url, int flags)
{
	int mux;
	QString pname(h->prot->name);
	if (pname.startsWith("lmmmuxi"))
		mux = pname.remove("lmmmuxi").toInt();
	if (pname.startsWith("lmmmuxo"))
		mux = pname.remove("lmmmuxo").toInt();
	h->priv_data = muxPriv.at(mux);
	fDebug("opening url %s", h->prot->name);
	return ((BaseLmmMux *)h->priv_data)->openUrl(url, flags);
}

int lmmUrlRead(URLContext *h, unsigned char *buf, int size)
{
	return ((BaseLmmMux *)h->priv_data)->readPacket(buf, size);
}

int lmmUrlWrite(URLContext *h, const unsigned char *buf, int size)
{
	return ((BaseLmmMux *)h->priv_data)->writePacket(buf, size);
}

int64_t lmmUrlSeek(URLContext *h, int64_t pos, int whence)
{
	(void)h;
	(void)pos;
	(void)whence;
	return -EINVAL;
}

int lmmUrlClose(URLContext *h)
{
	return ((BaseLmmMux *)h->priv_data)->closeUrl(h);
}

BaseLmmMux::BaseLmmMux(QObject *parent) :
	BaseLmmElement(parent)
{
	static bool avRegistered = false;
	if (!avRegistered) {
		mDebug("registering all formats");
		av_register_all();
		avRegistered = true;
	}

	muxNumber = muxPriv.size();
	/* register one channel for input, we need this to find stream info */
	URLProtocol *lmmUrlProtocol = new URLProtocol;
	memset(lmmUrlProtocol , 0 , sizeof(URLProtocol));
	QString pname = QString("lmmmuxi%1").arg(muxNumber);
	char *pnamep = new char[20];
	strcpy(pnamep, qPrintable(pname));
	lmmUrlProtocol->name = pnamep;
	lmmUrlProtocol->url_open = lmmUrlOpen;
	lmmUrlProtocol->url_read = lmmUrlRead;
	lmmUrlProtocol->url_write = lmmUrlWrite;
	lmmUrlProtocol->url_seek = lmmUrlSeek;
	lmmUrlProtocol->url_close = lmmUrlClose;
	av_register_protocol2(lmmUrlProtocol, sizeof (URLProtocol));

	/* register one channel for output, this is for the real stuff */
	lmmUrlProtocol = new URLProtocol;
	memset(lmmUrlProtocol , 0 , sizeof(URLProtocol));
	pname = QString("lmmmuxo%1").arg(muxNumber);
	pnamep = new char[20];
	strcpy(pnamep, qPrintable(pname));
	lmmUrlProtocol->name = pnamep;
	lmmUrlProtocol->url_open = lmmUrlOpen;
	lmmUrlProtocol->url_read = lmmUrlRead;
	lmmUrlProtocol->url_write = lmmUrlWrite;
	lmmUrlProtocol->url_seek = lmmUrlSeek;
	lmmUrlProtocol->url_close = lmmUrlClose;
	av_register_protocol2(lmmUrlProtocol, sizeof (URLProtocol));

	/* add us to mux list of static handlers */
	muxPriv << this;

	fmt = NULL;
	libavAnalayzeDuration = 5000000; /* this is ffmpeg default */
	muxOutputOpened = false;
	inputFmt = NULL;
}

int BaseLmmMux::start()
{
	context = NULL;
	inputContext = NULL;
	videoStreamIndex = audioStreamIndex = -1;
	audioStream = videoStream = NULL;
	inputContext = NULL;
	foundStreamInfo = false;
	//if (!sourceUrlName.contains("lmmmuxo"))
		//sourceUrlName = QString("lmmmuxo%1://%2").arg(muxNumber).arg(sourceUrlName);
	return BaseLmmElement::start();
}

int BaseLmmMux::stop()
{
	inputLock.lock();
	if (context) {
		av_write_trailer(context);
		for(uint i = 0; i < context->nb_streams; i++) {
			av_freep(&context->streams[i]->codec);
			av_freep(&context->streams[i]);
		}
		if (!fmt->flags & AVFMT_NOFILE)
			avio_close(context->pb);
		av_free(context);
		context = NULL;
	}
	if (inputContext) {
		av_close_input_file(inputContext);
		inputContext = NULL;
	}
	inputLock.unlock();

	return BaseLmmElement::stop();
}

int BaseLmmMux::findInputStreamInfo()
{
	mDebug("trying to find input stream info");
	if (!inputContext) {
		inputContext = avformat_alloc_context();
		if (!inputContext) {
			mDebug("error allocating input context");
			return -ENOMEM;
		}
		QString pname = QString("lmmmuxi%1://muxvideoinput").arg(muxNumber);
		int err = av_open_input_file(&inputContext, qPrintable(pname), inputFmt, 0, NULL);
		if (err) {
			mDebug("error opening input file %s", qPrintable(pname));
			return err;
		}
	}
	inputContext->max_analyze_duration = libavAnalayzeDuration;
	int err = av_find_stream_info(inputContext);
	if (err < 0) {
		mDebug("cannot find input stream info");
		return err;
	}
	mDebug("%d streams, %d programs present in the file", inputContext->nb_streams, inputContext->nb_programs);
	for (unsigned int i = 0; i < inputContext->nb_streams; ++i) {
		mDebug("stream: type %d", inputContext->streams[i]->codec->codec_type);
		if (inputContext->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
			videoStreamIndex = i;
		if (inputContext->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
			if (inputContext->streams[i]->codec->channels != 0
					|| inputContext->streams[i]->codec->sample_rate != 0)
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
		audioStream = inputContext->streams[audioStreamIndex];
		AVRational r = audioStream->time_base;
		//audioTimeBaseN = qint64(1000000000) * r.num / r.den;
		//mDebug("audioBase=%lld", audioTimeBaseN);
	}
	if (videoStreamIndex >= 0) {
		mDebug("video stream found at index %d", videoStreamIndex);
		videoStream = inputContext->streams[videoStreamIndex];
		AVRational r = videoStream->time_base;
		r = videoStream->time_base;
		//videoTimeBaseN = qint64(1000000000) * r.num / r.den;
		//mDebug("videoBase=%lld", videoTimeBaseN);
	}

	//streamPosition = 0;

	return 0;
}

#define PRINFO(__x) qDebug() << #__x << __x
void BaseLmmMux::printInputInfo()
{
	if (!inputContext)
		return;
	qDebug() << "class" << inputContext->av_class;
	qDebug() << "iformat" << inputContext->iformat;
	qDebug() << "oformat" << inputContext->iformat;
	qDebug() << "priv_data" << inputContext->priv_data;
	qDebug() << "pb" << inputContext->pb;
	qDebug() << "nb_streams" << inputContext->nb_streams;
	qDebug() << "timestamp" << inputContext->timestamp;
	qDebug() << "ctx_flags" << inputContext->ctx_flags;
	qDebug() << "packet_buffer" << inputContext->packet_buffer; //touched
	qDebug() << "start_time" << inputContext->start_time;
	qDebug() << "duration" << inputContext->duration;
	qDebug() << "file_size" << inputContext->file_size;
	qDebug() << "bit_rate" << inputContext->bit_rate;
	qDebug() << "cur_st" << inputContext->cur_st;
	qDebug() << "data_offset" << inputContext->data_offset;
	qDebug() << "mux_rate" << inputContext->mux_rate;
	qDebug() << "packet_size" << inputContext->packet_size;
	qDebug() << "preload" << inputContext->preload;
	qDebug() << "max_delay" << inputContext->max_delay;
	qDebug() << "loop_output" << inputContext->loop_output;
	qDebug() << "flags" << inputContext->flags;
	qDebug() << "loop_input" << inputContext->loop_input;
	qDebug() << "probesize" << inputContext->probesize;
	qDebug() << "max_analyze_duration" << inputContext->max_analyze_duration;
	qDebug() << "key" << inputContext->key;
	qDebug() << "keylen" << inputContext->keylen;
	qDebug() << "nb_programs" << inputContext->nb_programs;
	qDebug() << "video_codec_id" << inputContext->video_codec_id;
	qDebug() << "audio_codec_id" << inputContext->audio_codec_id;
	qDebug() << "subtitle_codec_id" << inputContext->subtitle_codec_id;
	qDebug() << "max_index_size" << inputContext->max_index_size;
	qDebug() << "max_picture_buffer" << inputContext->max_picture_buffer;
	qDebug() << "nb_chapters" << inputContext->nb_chapters;
	qDebug() << "raw_packet_buffer" << inputContext->raw_packet_buffer;
	qDebug() << "raw_packet_buffer_end" << inputContext->raw_packet_buffer_end;
	qDebug() << "packet_buffer_end" << inputContext->packet_buffer_end;
	qDebug() << "metadata" << inputContext->metadata;
	qDebug() << "raw_packet_buffer_remaining_size" << inputContext->raw_packet_buffer_remaining_size;
	qDebug() << "start_time_realtime" << inputContext->start_time_realtime;

	if (!videoStream)
		return;
	PRINFO(videoStream->codec->codec_id);
	PRINFO(videoStream->codec->codec_type);
	PRINFO(videoStream->codec->bit_rate);
	PRINFO(videoStream->codec->rc_max_rate);
	PRINFO(videoStream->codec->rc_buffer_size);
	PRINFO(videoStream->codec->extradata_size);
	PRINFO(videoStream->codec->time_base.num);
	PRINFO(videoStream->codec->time_base.den);
	PRINFO(videoStream->codec->ticks_per_frame);
	PRINFO(videoStream->codec->pix_fmt);
	PRINFO(videoStream->codec->width);
	PRINFO(videoStream->codec->height);
	PRINFO(videoStream->codec->has_b_frames);
	PRINFO(videoStream->time_base.num);
	PRINFO(videoStream->time_base.den);
}

RawBuffer BaseLmmMux::nextBuffer()
{
	if (!foundStreamInfo) {
		if (findInputStreamInfo()) {
			mDebug("error in input stream info");
		} else {
			foundStreamInfo = true;
			inputLock.lock();
			while (inputInfoBuffers.size())
				inputBuffers.prepend(inputInfoBuffers.takeLast());
			initMuxer();
			inputLock.unlock();
			emit inputInfoFound();
		}
	} else {
		inputLock.lock();
		if (inputBuffers.count() > 0) {
			mInfo("muxing next packet");
			RawBuffer buf = inputBuffers.takeFirst();
			AVPacket pckt;
			av_init_packet(&pckt);
			pckt.stream_index = 0;
			pckt.data = (uint8_t *)buf.constData();
			pckt.size = buf.size();
			AVRational avrat = context->streams[0]->time_base;
			pckt.pts = pckt.dts = streamTime->getCurrentTimeMili() * avrat.den / avrat.num / 1000;
			mInfo("writing next frame");
			av_write_frame(context, &pckt);
			if (!muxOutputOpened)
				outputBuffers << buf;
		}
		inputLock.unlock();
	}
	return BaseLmmElement::nextBuffer();
}

int BaseLmmMux::writePacket(const uint8_t *buffer, int buf_size)
{
	RawBuffer buf(mimeType(), (void *)buffer, buf_size);
	outputLock.lock();
	outputBuffers << buf;
	outputLock.unlock();
	return buf_size;
}

int BaseLmmMux::readPacket(uint8_t *buffer, int buf_size)
{
	mInfo("will read %d bytes into ffmpeg buffer", buf_size);
	/* This routine may be called before the stream started */
	int copied = 0, left = buf_size;
	inputLock.lock();
	while (inputBuffers.size()) {
		RawBuffer buf = inputBuffers.takeFirst();
		mInfo("using next input buffer, copied=%d left=%d buf.size()=%d", copied, left, buf.size());
		if (buf.size() > left) {
			memcpy(buffer + copied, buf.constData(), left);
			/* some data will left, put back to input buffers */
			RawBuffer iibuf(mimeType(), (void *)buf.constData(), left);
			inputInfoBuffers << iibuf;
			RawBuffer newbuf(mimeType(), (uchar *)buf.constData() + left, buf.size() - left);
			inputBuffers.prepend(newbuf);
			copied += left;
			left -= left;
			break;
		} else {
			memcpy(buffer + copied, buf.constData(), buf.size());
			copied += buf.size();
			left -= buf.size();
			inputInfoBuffers << buf;
		}
	}
	inputLock.unlock();
	mInfo("read %d bytes into ffmpeg buffer", copied);
	return copied;
}

int BaseLmmMux::openUrl(QString url, int)
{
	mDebug("opening %s", qPrintable(url));
	if (url.contains("lmmmuxo"))
		muxOutputOpened = true;
	return 0;
}

int BaseLmmMux::closeUrl(URLContext *)
{
	/* no need to do anything, stream will be closed later */
	return 0;
}

int BaseLmmMux::initMuxer()
{
	AVCodecContext *codec;
	AVCodecContext *icodec;
	uint64_t extra_size;
	int copy_tb = 1;

	int err = 0;
	if (!fmt)
		fmt = av_guess_format(NULL, qPrintable(sourceUrlName), NULL);
	if (!fmt) {
		mDebug("cannot guess codec format");
		return -EINVAL;
	}
	mInfo("format found for output: %s %s", fmt->long_name, fmt->mime_type);
	/*if (fmt->video_codec != CODEC_ID_H264) {
		mDebug("wrong video codec format(%d) detected: should be H264(%d)",
			   fmt->video_codec, CODEC_ID_H264);
		return -EINVAL;
	}*/

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
	icodec = videoStream->codec;
	codec->codec_id = icodec->codec_id;
	codec->codec_type = icodec->codec_type;
	codec->bit_rate = icodec->bit_rate;
	codec->rc_max_rate = icodec->rc_max_rate;
	codec->rc_buffer_size = icodec->rc_buffer_size;
	extra_size = (uint64_t)icodec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
	//TODO: we need to free codec extra data
	codec->extradata = (uint8_t *)av_mallocz(extra_size);
	memcpy(codec->extradata, icodec->extradata, icodec->extradata_size);
	copy_tb = 1;
	if (!copy_tb && av_q2d(icodec->time_base)*icodec->ticks_per_frame > av_q2d(videoStream->time_base) && av_q2d(videoStream->time_base) < 1.0/500) {
		codec->time_base = icodec->time_base;
		codec->time_base.num *= icodec->ticks_per_frame;
		av_reduce(&codec->time_base.num, &codec->time_base.den,
				  codec->time_base.num, codec->time_base.den, INT_MAX);
	} else
		codec->time_base = videoStream->time_base;
	codec->pix_fmt = icodec->pix_fmt;
	codec->width = icodec->width;
	codec->height = icodec->height;
	codec->has_b_frames = icodec->has_b_frames;
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
	//av_dump_format(context, 0, qPrintable(sourceUrlName), 1);

	if (!(fmt->flags & AVFMT_NOFILE) && avio_open(&context->pb, qPrintable(sourceUrlName), URL_WRONLY) < 0) {
		mDebug("error opening stream file");
		err = -EACCES;
		goto err_out1;
	}
	av_write_header(context);
	mInfo("output header written");

	return 0;
err_out1:
	avformat_free_context(context);
	return err;
}
