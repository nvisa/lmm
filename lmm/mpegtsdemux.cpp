#include "mpegtsdemux.h"
#include "circularbuffer.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "emdesk/debug.h"

#include <QFile>

static URLProtocol *lmmUrlProtocol = NULL;
/* TODO: Fix single instance MpegTsDemux */
static MpegTsDemux *demuxPriv = NULL;

static int lmmUrlOpen(URLContext *h, const char *url, int flags)
{
	qDebug() << __func__ << h << url << flags;
	h->priv_data = demuxPriv;

	return 0;
}

int lmmUrlRead(URLContext *h, unsigned char *buf, int size)
{
	//qDebug() << __func__  << h << buf << size;
	return ((MpegTsDemux *)h->priv_data)->readPacket(buf, size);
}

int lmmUrlWrite(URLContext *h, const unsigned char *buf, int size)
{
	qDebug() << __func__  << h << buf << size;
	return -EINVAL;
}

int64_t lmmUrlSeek(URLContext *h, int64_t pos, int whence)
{
	qDebug() << __func__  << h << pos << whence;
	return -EINVAL;
}

int lmmUrlClose(URLContext *h)
{
	qDebug() << __func__  << h;
	return 0;
}

MpegTsDemux::MpegTsDemux(QObject *parent) :
	BaseLmmDemux(parent)
{
	context = NULL;

	if (!lmmUrlProtocol) {
		lmmUrlProtocol = new URLProtocol;
		memset(lmmUrlProtocol , 0 , sizeof(URLProtocol));
		lmmUrlProtocol->name = "lmm";
		lmmUrlProtocol->url_open = lmmUrlOpen;
		lmmUrlProtocol->url_read = lmmUrlRead;
		lmmUrlProtocol->url_write = lmmUrlWrite;
		lmmUrlProtocol->url_seek = lmmUrlSeek;
		lmmUrlProtocol->url_close = lmmUrlClose;
		av_register_protocol2 (lmmUrlProtocol, sizeof (URLProtocol));
		demuxPriv = this;
	}

	mpegtsraw = av_iformat_next(NULL);
	while (mpegtsraw) {
		if (strcmp(mpegtsraw->name, "mpegtsraw") == 0)
			break;
		mpegtsraw = av_iformat_next(mpegtsraw);
	}
}

int MpegTsDemux::setSource(CircularBuffer *buf)
{
	circBuf = buf;
	return 0;
}

int MpegTsDemux::start()
{
	return BaseLmmDemux::start();
}

int MpegTsDemux::stop()
{
	av_close_input_file(context);
	context = NULL;
	return BaseLmmElement::stop();
}

int MpegTsDemux::seekTo(qint64)
{
	return -1;
}

int MpegTsDemux::demuxOne()
{
	if (context == NULL) {
		if (av_open_input_file(&context, "lmm://somechannel", NULL, 0, NULL))
			return -ENOENT;
		if (findStreamInfo())
			mDebug("error in stream info");
	}

	int bCnt = audioBufferCount();
	while (circBuf->usedSize() > 1024 * 100) {
		int err = BaseLmmDemux::demuxOne();
		if (err)
			return err;
	}

	if (audioBufferCount() - bCnt > 100) {
		qDeleteAll(audioBuffers);
		qDeleteAll(videoBuffers);
		audioBuffers.clear();
		videoBuffers.clear();
		streamTime->setStartTime(0);
		streamTime->setStartPts(0);
	}

	return 0;
}

int MpegTsDemux::readPacket(uint8_t *buf, int buf_size)
{
	while (buf_size > circBuf->usedSize()) {
		/* wait data to become availabe in circBuf */
		usleep(50000);
	}
	circBuf->lock();
	memcpy(buf, circBuf->getDataPointer(), buf_size);
	circBuf->useData(buf_size);
	circBuf->unlock();
	mInfo("read %d bytes into ffmpeg buffer", buf_size);
	return buf_size;
}
