#define __STDC_CONSTANT_MACROS

#include "baselmmdemux.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "debug.h"

#include <QSemaphore>

extern "C" {
	#include "libavformat/avformat.h"
	#include "libavformat/avio.h" /* for URLContext on x86 */
}

static void deletePacket(AVPacket *packet)
{
	if (packet->data != NULL)
		av_free_packet(packet);
	delete packet;
}

static QList<BaseLmmDemux *> demuxPriv;

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

	bufsem << new QSemaphore;

	demuxNumber = demuxPriv.size();
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

	/* add us to mux list of static handlers */
	demuxPriv << this;
}

qint64 BaseLmmDemux::getCurrentPosition()
{
	return streamTime->getCurrentTime();
}

qint64 BaseLmmDemux::getTotalDuration()
{
	if (videoStream)
		return videoStream->duration * videoTimeBaseN / 1000;
	if (audioStream)
		return audioStream->duration * audioTimeBaseN / 1000;
	return 0;
}

int BaseLmmDemux::findStreamInfo()
{
	/* context can be allocated by us or libavformat */
	int err = av_open_input_file(&context, qPrintable(sourceUrlName), av_find_input_format("mpegts"), 0, NULL);
	if (err)
		return err;

	context->max_analyze_duration = libavAnalayzeDuration;
	err = av_find_stream_info(context);
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

int BaseLmmDemux::setSource(QString filename)
{
	sourceUrlName = filename;
	if (sourceUrlName.contains("lmmdemuxi"))
		sourceUrlName = QString("lmmdemuxi%1://demuxvideoinput").arg(demuxNumber);
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
	static AVPacket *packet = NULL;
	if (packet)
		deletePacket(packet);
	packet = nextPacket();
	if (!packet)
		return -ENOENT;
	if (packet->stream_index == audioStreamIndex) {
		mInfo("new audio stream: size=%d", packet->size);
		if (demuxAudio) {
			RawBuffer buf("audio/x-raw-int", packet->data, packet->size);
			buf.setDuration(packet->duration * audioTimeBaseN / 1000);
			if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
				buf.setPts(packet->pts * audioTimeBaseN / 1000);
			} else {
				buf.setPts(-1);
			}
			if (packet->dts != int64_t(AV_NOPTS_VALUE)) {
				buf.setDts(packet->dts * audioTimeBaseN / 1000);
			} else {
				buf.setDts(-1);
			}
			audioBuffers << buf;
			bufsem[1]->release();
		}
	} else if (packet->stream_index == videoStreamIndex) {
		mInfo("new video stream: size=%d pts=%lld duration=%d dflags=%d", packet->size,
			   packet->pts == (int64_t)AV_NOPTS_VALUE ? -1 : packet->pts ,
			   packet->duration, packet->flags);
		if (demuxVideo) {
			RawBuffer buf("video/x-raw-yuv", packet->data, packet->size);
			buf.setDuration(packet->duration * videoTimeBaseN / 1000);
			if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
				buf.setPts(packet->pts * videoTimeBaseN / 1000);
			} else {
				buf.setPts(-1);
			}
			if (packet->dts != int64_t(AV_NOPTS_VALUE)) {
				buf.setDts(packet->dts * videoTimeBaseN / 1000);
			} else {
				buf.setDts(-1);
			}
			videoBuffers << buf;
			bufsem[0]->release();
		}
	}
	return 0;
}

int BaseLmmDemux::demuxOneBlocking()
{
	return demuxOne();
}

int BaseLmmDemux::flush()
{
	videoBuffers.clear();
	audioBuffers.clear();
	return BaseLmmElement::flush();
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
		inbufsem[0]->acquire();
		return readPacket(buffer, buf_size);
	}
	mInfo("read %d bytes into ffmpeg buffer", copied);
	return copied;
#else
	int copied = 0, left = buf_size;
	inbufsem[0]->acquire();
	inputLock.lock();
	RawBuffer buf = inputBuffers.takeFirst();
	if (buf.size() > left) {
		memcpy(buffer + copied, buf.constData(), left);
		/* some data will left, put back to input buffers */
		RawBuffer newbuf(mimeType(), (uchar *)buf.constData() + left, buf.size() - left);
		inputBuffers.prepend(newbuf);
		inbufsem[0]->release();
		copied += left;
		left -= left;
	} else {
		memcpy(buffer + copied, buf.constData(), buf.size());
		copied += buf.size();
		left -= buf.size();
	}
	inputLock.unlock();
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

RawBuffer BaseLmmDemux::nextAudioBuffer()
{
	if (audioBuffers.size()) {
		sentBufferCount++;
		return audioBuffers.takeFirst();
	}

	return RawBuffer();
}

RawBuffer BaseLmmDemux::nextAudioBufferBlocking()
{
	bufsem[1]->acquire();
	return nextAudioBuffer();
}

RawBuffer BaseLmmDemux::nextVideoBuffer()
{
	if (videoBuffers.size()) {
		sentBufferCount++;
		return videoBuffers.takeFirst();
	}

	return RawBuffer();
}

RawBuffer BaseLmmDemux::nextVideoBufferBlocking()
{
	bufsem[0]->acquire();
	return nextVideoBuffer();
}

int BaseLmmDemux::audioBufferCount()
{
	return audioBuffers.count();
}

int BaseLmmDemux::videoBufferCount()
{
	return videoBuffers.count();
}

int BaseLmmDemux::start()
{
	streamPosition = 0;
	videoStreamIndex = audioStreamIndex = -1;
	audioStream = videoStream = NULL;
	foundStreamInfo = false;
	return BaseLmmElement::start();
}

int BaseLmmDemux::stop()
{
	if (context) {
		av_close_input_file(context);
		context = NULL;
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
		int err = av_seek_frame(context, videoStreamIndex,
								pos * 1000 / videoTimeBaseN, flags);
		if (err < 0) {
			mDebug("error during seek");
			return err;
		}
	} else if (audioStreamIndex != -1) {
		int err = av_seek_frame(context, audioStreamIndex,
								pos * 1000 / audioTimeBaseN, flags);
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

AVPacket * BaseLmmDemux::nextPacket()
{
	AVPacket *packet = new AVPacket;
	if (av_read_frame(context, packet)) {
		deletePacket(packet);
		return NULL;
	}
	return packet;
}