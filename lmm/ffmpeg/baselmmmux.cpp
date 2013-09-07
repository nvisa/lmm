#define __STDC_CONSTANT_MACROS

#include "baselmmmux.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "debug.h"

#include <errno.h>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h> /* for URLContext on x86 */
	#include "ffcompat.h"
}

static QList<BaseLmmMux *> muxPriv;

#ifdef URL_RDONLY
static int lmmUrlOpen(URLContext *h, const char *url, int flags)
{
	int mux;
	fDebug("opening url %s", h->prot->name);
	QString pname(h->prot->name);
	if (pname.startsWith("lmmmuxi"))
		mux = pname.remove("lmmmuxi").toInt();
	if (pname.startsWith("lmmmuxo"))
		mux = pname.remove("lmmmuxo").toInt();
	h->priv_data = muxPriv.at(mux);
	return ((BaseLmmMux *)h->priv_data)->openUrl(url, flags);
}

static int lmmUrlRead(URLContext *h, unsigned char *buf, int size)
{
	return ((BaseLmmMux *)h->priv_data)->readPacket(buf, size);
}

static int lmmUrlWrite(URLContext *h, const unsigned char *buf, int size)
{
	return ((BaseLmmMux *)h->priv_data)->writePacket(buf, size);
}

static int64_t lmmUrlSeek(URLContext *h, int64_t pos, int whence)
{
	(void)h;
	(void)pos;
	(void)whence;
	return ((BaseLmmMux *)h->priv_data)->seekUrl(pos, whence);
}

static int lmmUrlClose(URLContext *h)
{
	return ((BaseLmmMux *)h->priv_data)->closeUrl(h);
}

#else

static int lmmUrlRead(void *opaque, uint8_t *buf, int buf_size)
{
	return ((BaseLmmMux *)opaque)->readPacket(buf, buf_size);
}

static int lmmUrlWrite(void *opaque, uint8_t *buf, int buf_size)
{
	return ((BaseLmmMux *)opaque)->writePacket(buf, buf_size);
}

static int64_t lmmUrlSeek(void *opaque, int64_t offset, int whence)
{
	Q_UNUSED(opaque);
	Q_UNUSED(offset);
	Q_UNUSED(whence);
	return -EINVAL;
}

#endif

/**
	\class BaseLmmMux

	\brief BaseLmmMux sinifi ffmpeg tabanli muxer elemanlarinin turetildigi baz
	siniftir.

	BaseLmmMux ffmpeg libavformat kutuphanesi uzerinde calisan bir siniftir. Uygulamada
	bu sinif direk olarak kullanilmak icin tasarlanmamistir, ozel bir muxer sinifi
	yaratmaniz gereklidir.

	BaseLmmMux sinifi giris ve cikislarini addBuffer() ve nextBuffer() fonksiyonlari
	ile alabilir. Fakat bazi muxer siniflari direk olarak diske yazabilir ya da ag akisi
	olarak gonderebilir. Bunun icin ilgili sinifin dokumantasyonuna bakabilirsiniz.
	Fakat tampon ciktisi verebilen siniflari tampon ciktisi verecek sekilde ayarlamak
	icin setOutputFilename() fonksiyonuna 'lmmmuxo' gecebilirsiniz:

	\code
	BaseLmmMux *mux = new SomeMuxer();
	mux->setOutputFilename("lmmmuxo");
	\endcode

	\section Kalitma BaseLmmMux Sinifindan Kalitma

	Bu siniftan kalitirken en az mimeType() fonksiyonunu implemente etmelisiniz. Bu tip
	yeni yaratilan tamponlarda kullanilacaktir.

	findInputStreamInfo() ve initMuxer() fonksiyonlari muxer ozelliklerini belirlemek ve
	ayarlamak icin kullanilir. Alt siniflar bu fonksiyonlari daha hizli muxer olusturumu
	icin kullanabilir. libavAnalayzeDuration degiskeni findInputStream()'in ne kadar sure
	ile gelen tamponlari format belirlemek icin arastiracagini belirtir. Gerekli
	parametreleri bulabilmek icin printInputInfo() fonksiyonunu kullanabilirsiniz.

	packetTimeStamp() fonksiyonunu yeniden gerceklerseniz, olusturulan tamponlarin pts/dts
	degerlerini atayabilirisiniz. Varsayilan olarak libavformat icinde tavsiye method
	kullanilir.

	timebaseNum() ve timebaseDenom() fonksiyonlari initMuxer() tarafindan olusturulan
	codec iceriginin zamanlama degerlerini atamak icin kullanilir, yeniden implemente ederek
	fps degerini degistirebilirsiniz.

	\ingroup ffmpeg
*/
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
#ifdef URL_RDONLY
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
#else
	avioBufferSizeIn = 4096;
	avioBufferIn = (uchar *)av_malloc(avioBufferSizeIn);
	avioCtxIn = avio_alloc_context(avioBufferIn, avioBufferSizeIn, 0, this, lmmUrlRead, lmmUrlWrite, lmmUrlSeek);

	avioBufferSizeOut = 4096;
	avioBufferOut = (uchar *)av_malloc(avioBufferSizeOut);
	avioCtxOut = avio_alloc_context(avioBufferOut, avioBufferSizeOut, 0, this, lmmUrlRead, lmmUrlWrite, lmmUrlSeek);
#endif
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
	if (sourceUrlName.contains("lmmmuxo"))
		sourceUrlName = QString("lmmmuxo%1://muxvideooutput").arg(muxNumber);
	return BaseLmmElement::start();
}

int BaseLmmMux::stop()
{
	mutex.lock();
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
	mutex.unlock();

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
#ifdef URL_RDONLY
		int err = av_open_input_file(&inputContext, qPrintable(pname), inputFmt, 0, NULL);
#else
		inputContext->pb = (AVIOContext *)avioCtxIn;
		int err = avformat_open_input(&inputContext, qPrintable(pname), inputFmt, NULL);
#endif
		if (err) {
			mDebug("error %d opening input file %s", err, qPrintable(pname));
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
#ifdef URL_RDONLY
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
#endif
}

int BaseLmmMux::processBuffer(RawBuffer buf)
{
	return processBuffer(0, buf);
}

int BaseLmmMux::processBuffer(int ch, RawBuffer buf)
{
	if (!foundStreamInfo) {
		if (findInputStreamInfo()) {
			mDebug("error in input stream info");
		} else {
			/* TODO: We lose input info buffers here, prior to BaseLmmElement
			 * re-work we had this:
				while (inputInfoBuffers.size())
					inputBuffers.prepend(inputInfoBuffers.takeLast());
			*/
			mutex.lock();
			int err = 0;
			if (!context)
				err = initMuxer();
			mutex.unlock();
			if (err)
				return err;
			foundStreamInfo = true;
			emit inputInfoFound();
		}
	} else {
		mInfo("muxing next packet");
		AVPacket pckt;
		av_init_packet(&pckt);
		pckt.stream_index = ch;
		pckt.data = (uint8_t *)buf.constData();
		pckt.size = buf.size();
		if (ch == 0) {
			int ft = buf.getBufferParameter("frameType").toInt();
			if (!ft) {
				pckt.flags |= AV_PKT_FLAG_KEY;
			}
		}
		pckt.pts = pckt.dts = packetTimestamp(ch);
		mInfo("writing next frame %d %lld", buf.size(), pckt.dts);
		mutex.lock();
		av_write_frame(context, &pckt);
		muxedBufferCount[ch]++;
		mutex.unlock();
	}
	return 0;
}

qint64 BaseLmmMux::packetTimestamp(int stream)
{
	return muxedBufferCount[stream];
}

int BaseLmmMux::timebaseNum()
{
	return 1;
}

int BaseLmmMux::timebaseDenom()
{
	return 25;
}

/**
 * @brief libavformat tamponlarini diske yazar.
 * @return Hata yoksa '0', hata durumunda negatif hata kodu.
 *
 * Bazi muxerlar uzerinde farkli araliklarla bu fonksiyonu cagirmaniz
 * gerekebilir. Ornegin MP4/QT icinde moov atomlarinin yazilmasi icin
 * bu fonksiyonu cagirabilirsiniz.
 */
int BaseLmmMux::sync()
{
	mutex.lock();
	av_write_trailer(context);
	av_write_header(context);
	mutex.unlock();
	return 0;
}

/**
 * @brief Muxer ciktisini secmek icin kullanabilirsiniz.
 * @param filename Dosya ismi.
 * @return Hata yoksa '0', hata durumunda negatif hata kodu.
 *
 * Muxer cikisinin gonderilecegi adresi belirler. Eger 'lmmmuxo'
 * gecerseniz elemandan ciktilari nextBuffer() fonksiyonu ile
 * alabilirsiniz.
 */
int BaseLmmMux::setOutputFilename(QString filename)
{
	sourceUrlName = filename;
	return 0;
}

/**
 * @brief libavformat tarafindan kullanilir, cagirmayiniz.
 * @param buffer
 * @param buf_size
 * @return
 */
int BaseLmmMux::writePacket(const uint8_t *buffer, int buf_size)
{
	RawBuffer buf(mimeType(), (void *)buffer, buf_size);
	newOutputBuffer(0, buf);
	currUrlPos += buf_size;
	return buf_size;
}

/**
 * @brief libavformat tarafindan kullanilir, cagirmayiniz.
 * @param buffer
 * @param buf_size
 * @return
 */
int BaseLmmMux::readPacket(uint8_t *buffer, int buf_size)
{
	mInfo("will read %d bytes into ffmpeg buffer", buf_size);
	/* This routine may be called before the stream started */
	int copied = 0, left = buf_size;
	if (!acquireInputSem(0))
		return -EINVAL;
	RawBuffer buf = takeInputBuffer(0);
	while (buf.size()) {
		mInfo("using next input buffer, copied=%d left=%d buf.size()=%d", copied, left, buf.size());
		if (buf.size() > left) {
			memcpy(buffer + copied, buf.constData(), left);
			copied += left;
			left -= left;
			break;
		} else {
			memcpy(buffer + copied, buf.constData(), buf.size());
			copied += buf.size();
			left -= buf.size();
		}
		if (!acquireInputSem(0))
			return -EINVAL;
		buf = takeInputBuffer(0);
	}
	mInfo("read %d bytes into ffmpeg buffer", copied);
	return copied;
}

/**
 * @brief libavformat tarafindan kullanilir, cagirmayiniz.
 * @param url
 * @return
 */
int BaseLmmMux::openUrl(QString url, int)
{
	mDebug("opening %s", qPrintable(url));
	if (url.contains("lmmmuxo"))
		muxOutputOpened = true;
	currUrlPos = 0;
	return 0;
}

/**
 * @brief libavformat tarafindan kullanilir, cagirmayiniz.
 * @return
 */
int BaseLmmMux::closeUrl(URLContext *)
{
	/* no need to do anything, stream will be closed later */
	return 0;
}

int64_t BaseLmmMux::seekUrl(int64_t pos, int whence)
{
	Q_UNUSED(pos);
	Q_UNUSED(whence);
	return -EINVAL;
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
	codec->time_base.num = timebaseNum();
	codec->time_base.den = timebaseDenom();
#ifdef URL_RDONLY
	st->stream_copy = 1;
#endif
	st->pts.num = codec->time_base.num;
	st->pts.den = codec->time_base.den;
	st->time_base.num = codec->time_base.num;
	st->time_base.den = codec->time_base.den;
	if (context->oformat->flags & AVFMT_GLOBALHEADER)
		codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	mInfo("output codec parameters adjusted");

#ifdef URL_RDONLY
	context->timestamp = 0;
	context->preload = 0.5 * AV_TIME_BASE; //from ffmpeg source
	context->loop_output = 0;
#else
	context->probesize = 0.5 * AV_TIME_BASE; //from ffmpeg source
#endif
	context->max_delay = 0.7 * AV_TIME_BASE; //from ffmpeg source
	context->flags |= AVFMT_FLAG_NONBLOCK;
	//av_dump_format(context, 0, qPrintable(sourceUrlName), 1);

	if (!(fmt->flags & AVFMT_NOFILE) && avio_open(&context->pb, qPrintable(sourceUrlName), URL_WRONLY) < 0) {
		mDebug("error opening stream file");
		err = -EACCES;
		goto err_out1;
	}
	err = av_write_header(context);
	mInfo("output header written, status %d", err);

	return 0;
err_out1:
	avformat_free_context(context);
	return err;
}
