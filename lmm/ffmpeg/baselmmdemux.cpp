#define __STDC_CONSTANT_MACROS

#include "baselmmdemux.h"
#include "rawbuffer.h"
#include "ffmpegbuffer.h"
#include "streamtime.h"
#include "debug.h"
#include "tools/unittimestat.h"

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
	#include "ffcompat.h"
}

static void deletePacket(AVPacket *packet)
{
	if (packet->data != NULL)
		av_free_packet(packet);
	delete packet;
}

static QList<BaseLmmDemux *> demuxPriv;

#ifdef URL_RDONLY
static int lmmUrlOpen(URLContext *h, const char *url, int flags)
{
	int mux;
	fDebug("opening url %s", h->prot->name);
	QString pname(h->prot->name);
	if (pname.startsWith("lmmdemuxi"))
		mux = pname.remove("lmmdemuxi").toInt();
	h->priv_data = demuxPriv.at(mux);
	return ((BaseLmmDemux *)h->priv_data)->openUrl(url, flags);
}

static int lmmUrlRead(URLContext *h, unsigned char *buf, int size)
{
	return ((BaseLmmDemux *)h->priv_data)->readPacket(buf, size);
}

static int lmmUrlWrite(URLContext *h, const unsigned char *buf, int size)
{
	return ((BaseLmmDemux *)h->priv_data)->writePacket(buf, size);
}

static int64_t lmmUrlSeek(URLContext *h, int64_t pos, int whence)
{
	(void)h;
	(void)pos;
	(void)whence;
	return -EINVAL;
}

static int lmmUrlClose(URLContext *h)
{
	return ((BaseLmmDemux *)h->priv_data)->closeUrl(h);
}

#else
static int lmmUrlRead(void *opaque, uint8_t *buf, int buf_size)
{
	return ((BaseLmmDemux *)opaque)->readPacket(buf, buf_size);
}

static int lmmUrlWrite(void *opaque, uint8_t *buf, int buf_size)
{
	return ((BaseLmmDemux *)opaque)->writePacket(buf, buf_size);
}

static int64_t lmmUrlSeek(void *opaque, int64_t offset, int whence)
{
	Q_UNUSED(opaque);
	Q_UNUSED(offset);
	Q_UNUSED(whence);
	return -EINVAL;
}
#endif

int interrupCallback(void *opaque)
{
	return ((BaseLmmDemux *)opaque)->isAvformatContextDone() ? 1 : 0;
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
	loopFile = false;

	demuxNumber = demuxPriv.size();

#ifdef URL_RDONLY
	/* register one channel for input, we need this to find stream info */
	URLProtocol *lmmUrlProtocol = new URLProtocol;
	memset(lmmUrlProtocol , 0 , sizeof(URLProtocol));
	QString pname = QString("lmmdemuxi%1").arg(demuxNumber);
	char *pnamep = new char[20];
	strcpy(pnamep, qPrintable(pname));
	lmmUrlProtocol->name = pnamep;
	lmmUrlProtocol->url_open = lmmUrlOpen;
	lmmUrlProtocol->url_read = lmmUrlRead;
	lmmUrlProtocol->url_write = lmmUrlWrite;
	lmmUrlProtocol->url_seek = lmmUrlSeek;
	lmmUrlProtocol->url_close = lmmUrlClose;
	av_register_protocol2(lmmUrlProtocol, sizeof (URLProtocol));
#else
	avioBufferSize = 4096;
	avioBuffer = (uchar *)av_malloc(avioBufferSize);
	avioCtx = avio_alloc_context(avioBuffer, avioBufferSize, 0, this, lmmUrlRead, lmmUrlWrite, lmmUrlSeek);
#endif
	/* add us to mux list of static handlers */
	demuxPriv << this;
}

qint64 BaseLmmDemux::getCurrentPosition()
{
	return streamTime->getCurrentTime();
}

qint64 BaseLmmDemux::getTotalDuration()
{
	qint64 val = -1;
	conlock.lock();
	if (context && context->duration)
		val = context->duration;
	else if (videoStream)
		val = videoStream->duration * videoTimeBaseN / 1000;
	else if (audioStream)
		val = audioStream->duration * audioTimeBaseN / 1000;
	conlock.unlock();
	return val;
}

int BaseLmmDemux::findStreamInfo()
{
	conlock.lock();
	/* context can be allocated by us or libavformat */
#ifdef URL_RDONLY
	int err = av_open_input_file(&context, qPrintable(sourceUrlName), NULL, 0, NULL);
#else
	context = avformat_alloc_context();
	if (!context) {
		mDebug("error allocating input context");
		return -ENOMEM;
	}
	if (sourceUrlName.contains("lmmdemuxi")) {
		context->pb = (AVIOContext *)avioCtx;
	}
	AVDictionary* options = NULL;
	if (getParameter("rtsp_transport").isValid()) {
		av_dict_set(&options, "rtsp_transport", qPrintable(getParameter("rtsp_transport").toString()), 0);
	}
	context->interrupt_callback.callback = interrupCallback;
	context->interrupt_callback.opaque = this;
	int err = avformat_open_input(&context, qPrintable(sourceUrlName), NULL, &options);
#endif
	if (err) {
		mDebug("cannot open input file '%s', errorno is %d", qPrintable(sourceUrlName), err);
		conlock.unlock();
		return err;
	}

	context->max_analyze_duration = libavAnalayzeDuration;
	conlock.unlock();
	err = avformat_find_stream_info(context, NULL);
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

QString BaseLmmDemux::mimeType()
{
	return "unknown";
}

int BaseLmmDemux::setSource(QString filename)
{
	sourceUrlName = filename;
	if (sourceUrlName.contains("lmmdemuxi"))
		sourceUrlName = QString("lmmdemuxi%1://demuxvideoinput").arg(demuxNumber);
	return 0;
}

int BaseLmmDemux::getFrameDuration(int st)
{
	if (!context || context->nb_streams <= (uint)st)
		return 0;
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
	AVPacket *packet = NULL;
	conlock.lock();
	packet = nextPacket();
	if (!packet) {
		if (loopFile) {
			avformat_seek_file(context, videoStreamIndex, 0, 0, 0, AVSEEK_FLAG_BACKWARD);
			demuxedCount[videoStreamIndex] = 0;
			conlock.unlock();
			return 0;
		}
		conlock.unlock();
		if (demuxVideo)
			newOutputBuffer(0, RawBuffer());
		if (demuxAudio)
			newOutputBuffer(1, RawBuffer());
		return -ENOENT;
	}
	if (packet->stream_index == audioStreamIndex) {
		mInfo("new audio stream: size=%d", packet->size);
		if (demuxAudio) {
			FFmpegBuffer buf("audio/mpeg", packet);
			buf.pars()->duration = packet->duration * audioTimeBaseN / 1000;
			if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
				buf.pars()->pts = packet->pts * audioTimeBaseN / 1000;
			} else {
				buf.pars()->pts = -1;
			}
			buf.pars()->streamBufferNo = demuxedCount[audioStreamIndex]++;
			buf.setCodecContext(getAudioCodecContext());
			newOutputBuffer(1, buf);
		} else
			deletePacket(packet);
	} else if (packet->stream_index == videoStreamIndex) {
		mInfo("new video stream: size=%d pts=%lld duration=%d dflags=%d", packet->size,
			   packet->pts == (int64_t)AV_NOPTS_VALUE ? -1 : packet->pts ,
			   packet->duration, packet->flags);
		if (demuxVideo) {
			FFmpegBuffer buf("video/mpeg", packet);
			buf.pars()->duration = packet->duration * videoTimeBaseN / 1000;
			if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
				buf.pars()->pts = packet->pts * videoTimeBaseN / 1000;
			} else {
				buf.pars()->pts = -1;
			}
			buf.pars()->streamBufferNo = demuxedCount[videoStreamIndex]++;
			if (packet->flags & AV_PKT_FLAG_KEY)
				buf.pars()->frameType = 0;
			else
				buf.pars()->frameType = 1;
			AVCodecContext *ctx = context->streams[packet->stream_index]->codec;
			if (ctx) {
				buf.pars()->videoWidth = ctx->width;
				buf.pars()->videoHeight = ctx->height;
			}
			buf.setCodecContext(getVideoCodecContext());
			newOutputBuffer(0, buf);
		} else
			deletePacket(packet);
	}
	conlock.unlock();
	return 0;
}

int BaseLmmDemux::processBuffer(const RawBuffer &buf)
{
	return demuxOne();
}

int BaseLmmDemux::getDemuxedCount()
{
	return demuxedCount[videoStreamIndex];
}

int BaseLmmDemux::processBlocking(int ch)
{
	int ret = 0;
	processTimeStat->startStat();
	//notifyEvent(EV_PROCESS, buf);
	ret = demuxOne();
	processTimeStat->addStat();
	if (ret == -EAGAIN)
		return processBlocking(ch);

	return ret;
}

AVCodecContext * BaseLmmDemux::getVideoCodecContext()
{
	return context->streams[videoStreamIndex]->codec;
}

AVCodecContext *BaseLmmDemux::getAudioCodecContext()
{
	return context->streams[audioStreamIndex]->codec;
}

void BaseLmmDemux::setLoopFile(bool value)
{
	loopFile = value;
}

int BaseLmmDemux::getAudioSampleRate()
{
	if (audioStream)
		return audioStream->codec->sample_rate;
	return 0;
}

int BaseLmmDemux::readPacket(uint8_t *buffer, int buf_size)
{
#if 0
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
			RawBuffer newbuf(mimeType(), (uchar *)buf.constData() + left, buf.size() - left);
			inputBuffers.prepend(newbuf);
			copied += left;
			left -= left;
			break;
		} else {
			memcpy(buffer + copied, buf.constData(), buf.size());
			copied += buf.size();
			left -= buf.size();
		}
	}
	inputLock.unlock();
	if (!copied) {
		if (!acquireInputSem(0))
			return -EINVAL;
		return readPacket(buffer, buf_size);
	}
	mInfo("read %d bytes into ffmpeg buffer", copied);
	return copied;
#else
	int copied = 0, left = buf_size;
	/*if (!acquireInputSem(0)) {
		return -EINVAL;
	}*/
	RawBuffer buf ;//= takeInputBuffer(0);
	if (buf.size() > left) {
		memcpy(buffer + copied, buf.constData(), left);
		/* some data will left, put back to input buffers */
		RawBuffer newbuf(mimeType(), (uchar *)buf.constData() + left, buf.size() - left);
		//prependInputBuffer(0, newbuf);
		copied += left;
		left -= left;
	} else {
		memcpy(buffer + copied, buf.constData(), buf.size());
		copied += buf.size();
		left -= buf.size();
	}
	return copied;
#endif
}

int BaseLmmDemux::writePacket(const uint8_t *buffer, int buf_size)
{
	qDebug("write %d", buf_size);
	return buf_size;
}

int BaseLmmDemux::openUrl(QString url, int)
{
	qDebug("opening %s", qPrintable(url));
	return 0;
}

int BaseLmmDemux::closeUrl(URLContext *h)
{
	/* no need to do anything, stream will be closed later */
	return 0;
}

int BaseLmmDemux::start()
{
	if (context) {
		conlock.lock();
		avformat_close_input(&context);
		context = NULL;
		mDebug("bye bye to context");
		conlock.unlock();
	}
	unblockContext = false;
	demuxedCount.clear();
	streamPosition = 0;
	videoStreamIndex = audioStreamIndex = -1;
	audioStream = videoStream = NULL;
	foundStreamInfo = false;
	return BaseLmmElement::start();
}

int BaseLmmDemux::stop()
{
	if (sourceUrlName.contains("rtsp")) {
		unblockContext = true;
		conlock.lock();
		avformat_close_input(&context);
		context = NULL;
		conlock.unlock();
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
		conlock.lock();
		int err = av_seek_frame(context, videoStreamIndex,
								pos * 1000 / videoTimeBaseN, flags);
		conlock.unlock();
		if (err < 0) {
			mDebug("error during seek");
			return err;
		}
	} else if (audioStreamIndex != -1) {
		conlock.lock();
		int err = av_seek_frame(context, audioStreamIndex,
								pos * 1000 / audioTimeBaseN, flags);
		conlock.unlock();
		if (err < 0) {
			mDebug("error during seek");
			return err;
		}
	} else {
		conlock.lock();
		int err = av_seek_frame(context, -1, pos, flags);
		conlock.unlock();
		if (err < 0) {
			mDebug("error during seek");
			return err;
		}
	}
	flush();
	return 0;
}

AVPacket * BaseLmmDemux::nextPacket()
{
	AVPacket *packet = new AVPacket;
	//conlock.lock();
	av_init_packet(packet);
	packet->data = NULL;
	packet->size = 0;
	if (context && av_read_frame(context, packet)) {
		deletePacket(packet);
		//conlock.unlock();
		return NULL;
	}
	//conlock.unlock();
	return packet;
}
