#define __STDC_CONSTANT_MACROS

#include "baselmmmux.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "emdesk/debug.h"

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

	return BaseLmmElement::stop();
}

int BaseLmmMux::findInputStreamInfo()
{
	mDebug("trying to find input stream info");
	if (!inputContext) {
		inputContext = avformat_alloc_context();
		if (!inputContext)
			return -ENOMEM;
		QString pname = QString("lmmmuxi%1://muxvideoinput").arg(muxNumber);
		int err = av_open_input_file(&inputContext, qPrintable(pname), NULL, 0, NULL);
		if (err)
			return err;
	}
	inputContext->max_analyze_duration = libavAnalayzeDuration;
	int err = av_find_stream_info(inputContext);
	if (err < 0)
		return err;
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

RawBuffer BaseLmmMux::nextBuffer()
{
	if (!foundStreamInfo) {
		if (findInputStreamInfo()) {
			mDebug("error in input stream info");
		} else {
			foundStreamInfo = true;
			while (inputInfoBuffers.size())
				inputBuffers.prepend(inputInfoBuffers.takeLast());
			//if (debugMessagesAvailable())
				//av_dump_format(context, 0, qPrintable(sourceUrlName), false);
			//emit streamInfoFound();
			initMuxer();
		}
	} else {
		if (inputBuffers.count() > 0) {
			mDebug("muxing next packet");
			//AVPacket *pckt = new AVPacket;
			//av_read_frame(inputContext, pckt);
			/*context->
			//pckt.pts =
			pckt.stream_index = 0;
			pckt.data = buf.constData();*/

			RawBuffer buf = inputBuffers.takeFirst();
			AVPacket pckt;
			av_init_packet(&pckt);
			pckt.stream_index = 0;
			pckt.data = (uint8_t *)buf.constData();
			pckt.size = buf.size();
			av_write_frame(context, &pckt);

		}
	}
	return BaseLmmElement::nextBuffer();
}

int BaseLmmMux::writePacket(const uint8_t *buffer, int buf_size)
{
	RawBuffer buf((void *)buffer, buf_size);
	outputBuffers << buf;
	return buf_size;
}

int BaseLmmMux::readPacket(uint8_t *buffer, int buf_size)
{
	mInfo("will read %d bytes into ffmpeg buffer", buf_size);
	/* This routine may be called before the stream started */
	int copied = 0, left = buf_size;
	while (inputBuffers.size()) {
		RawBuffer buf = inputBuffers.takeFirst();
		mInfo("using next input buffer, copied=%d left=%d buf.size()=%d", copied, left, buf.size());
		if (buf.size() > left) {
			memcpy(buffer + copied, buf.constData(), left);
			/* some data will left, put back to input buffers */
			RawBuffer iibuf((void *)buf.constData(), left);
			inputInfoBuffers << iibuf;
			RawBuffer newbuf((uchar *)buf.constData() + left, buf.size() - left);
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
	/*if (fd < 0)
		start();
	QTime timeout; timeout.start();
	while (buf_size > circBuf->usedSize()) {
		usleep(50000);
		if (timeout.elapsed() > 10000)
			return -ENOENT;
	}
	circBuf->lock();
	memcpy(buf, circBuf->getDataPointer(), buf_size);
	circBuf->useData(buf_size);
	circBuf->unlock();*/
	mInfo("read %d bytes into ffmpeg buffer", copied);
	return copied;
}

int BaseLmmMux::openUrl(QString url, int)
{
	/*url.remove("lmm://");
	QStringList fields = url.split(":");
	QString stream = "tv";
	QString channel;
	if (fields.size() == 2) {
		stream = fields[0];
		channel = fields[1];
	} else
		channel = fields[0];
	if (!DVBUtils::tuneToChannel(channel))
		return -EINVAL;
	struct dvb_ch_info info = DVBUtils::currentChannelInfo();
	apid = info.apid;
	vpid = info.vpid;
	sid = info.spid;
	if (sid == 0)
		sid = 1;
	if (info.freq == 11981)
		sid = 2;
	pmt = -1;
	pcr = -1;*/
	mDebug("opening %s", qPrintable(url));
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
	if (context->oformat->flags & AVFMT_GLOBALHEADER)
		codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	mInfo("output codec parameters adjusted");

	context->timestamp = 0;
	/*AVFormatParameters *ap;
	memset(ap, 0, sizeof(*ap));
	if (av_set_parameters(context, ap) < 0) {
		mDebug("error setting stream paramters");
		err = -EINVAL;
		goto err_out1;
	}
	mInfo("context parameters set");*/

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
