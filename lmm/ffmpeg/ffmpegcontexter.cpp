#include "ffmpegcontexter.h"

extern "C" {
	#include <libavformat/avio.h>
	#include <libavformat/avformat.h>
}

class FFmpegContexterPriv
{
public:
	FFmpegContexterPriv(FFmpegContexter *parent)
	{
		int bufsize = 4096;
		uchar *buffer = (uchar *)av_malloc(bufsize);
		ioctx = avio_alloc_context(buffer, bufsize, 0, parent,
								   &FFmpegContexter::read_packet,
								   NULL, NULL);

		ctx = avformat_alloc_context();
		ctx->pb = ioctx;
	}

	int open()
	{
		return avformat_open_input(&ctx, NULL, NULL, NULL);
	}

	void close()
	{
		avformat_close_input(&ctx);
	}

	~FFmpegContexterPriv()
	{
		av_free(ioctx->buffer);
		av_free(ioctx);
	}

	AVFormatContext *ctx;
	AVIOContext *ioctx;
	//uchar *buffer;
	//int bufsize;
};

FFmpegContexter::FFmpegContexter(QObject *parent)
	: BaseLmmElement(parent)
{

}

int FFmpegContexter::start()
{
	FFmpegContexterPriv *priv = new FFmpegContexterPriv(this);
	int err = priv->open();
	if (err)
		return err;
	return BaseLmmElement::start();
}

int FFmpegContexter::processBuffer(int ch, const RawBuffer &buf)
{
	Q_UNUSED(ch);
	buflock.lock();
	inputBuffers << buf;
	buflock.unlock();
	return 0;
}

int FFmpegContexter::read_packet(void *opaque, uchar *buf, int size)
{
	FFmpegContexter *ctx = (FFmpegContexter *)opaque;
	return ctx->readPacket(buf, size);
}

int FFmpegContexter::readPacket(uchar *buffer, int size)
{
	int off = 0, left = size;
	buflock.lock();
	while (left) {
		if (!inputBuffers.size()) {
			buflock.unlock();
			usleep(10000);
			buflock.lock();
			continue;
		}
		RawBuffer buf = inputBuffers.takeFirst();
		if (buf.size() <= left) {
			memcpy(&buffer[off], buf.constData(), buf.size());
			off += buf.size();
			left -= buf.size();
		} else {
			memcpy(&buffer[off], buf.constData(), left);
			RawBuffer bufn(buf.getMimeType(), buf.size() - left);
			memcpy(bufn.constData(), (const uchar *)buf.constData() + left, buf.size() - left);
			inputBuffers << bufn;
			off += left;
			left -= left;
		}
	}

}

