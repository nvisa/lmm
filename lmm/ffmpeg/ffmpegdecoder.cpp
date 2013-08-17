#include "ffmpegdecoder.h"

#include <lmm/ffmpeg/ffmpegbuffer.h>
#include <lmm/lmmbufferpool.h>

#include <lmm/debug.h>

extern "C" {
	#define __STDC_CONSTANT_MACROS
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libswscale/swscale.h>
}

FFmpegDecoder::FFmpegDecoder(QObject *parent) :
	BaseLmmDecoder(parent)
{
	codecCtx = NULL;
	pool = new LmmBufferPool(this);
}

int FFmpegDecoder::setStream(AVCodecContext *stream)
{
	codec = avcodec_find_decoder(stream->codec_id);
	if (!codec) {
		mDebug("cannot find decoder for codec %s", stream->codec_name);
		return -ENOENT;
	}
	codecCtx = stream;
	int err = avcodec_open2(stream, codec, NULL);
	if (err) {
		mDebug("cannot open codec");
		return err;
	}
	return 0;
}

int FFmpegDecoder::startDecoding()
{
	avFrame = avcodec_alloc_frame();
	swsCtx = NULL;
	decodeCount = 0;
	return 0;
}

int FFmpegDecoder::stopDecoding()
{
	codecCtx = NULL;
	return 0;
}

int FFmpegDecoder::decode()
{
	mInfo("starting decode operation");
	RawBuffer buf;
	if (inputBuffers.size()) {
		inputLock.lock();
		buf = inputBuffers.takeFirst();
		inputLock.unlock();
		FFmpegBuffer *ffbuf = (FFmpegBuffer *)&buf;
		AVPacket *packet = ffbuf->getAVPacket();
		int finished;
		int bytes = avcodec_decode_video2(codecCtx, avFrame, &finished, packet);
		if (bytes < 0) {
			mDebug("error %d while decoding frame", bytes);
			return bytes;
		}
		if (finished) {
			if (codecCtx->pix_fmt == PIX_FMT_YUV420P || codecCtx->pix_fmt == PIX_FMT_YUVJ420P) {
				mInfo("decoded video frame");
				if (!swsCtx) {
					swsCtx = sws_getContext(codecCtx->width, codecCtx->height, static_cast<PixelFormat>(codecCtx->pix_fmt),
											codecCtx->width, codecCtx->height, PIX_FMT_RGB24, SWS_BICUBIC
											, NULL, NULL, NULL);
					for (int i = 0; i < 15; i++) {
						FFmpegBuffer buf("video/x-raw-rgb", codecCtx->width, codecCtx->height, this);
						buf.addBufferParameter("poolIndex", i);
						pool->addBuffer(buf);
						poolBuffers.insert(i, buf);
					}
				}
				RawBuffer poolbuf = pool->take();
				AVFrame *frame = (AVFrame *)poolbuf.getBufferParameter("AVFrame").toULongLong();
				sws_scale(swsCtx, avFrame->data, avFrame->linesize, 0, codecCtx->height,
						  frame->data, frame->linesize);
				RawBuffer outbuf(QString("video/x-raw-rgb"),
							  (const void *)poolbuf.data(), poolbuf.size(), this);
				outbuf.addBufferParameter("width", codecCtx->width);
				outbuf.addBufferParameter("height", codecCtx->height);
				outbuf.addBufferParameter("avFrame", (quintptr)frame);
				outbuf.addBufferParameter("poolIndex", poolbuf.getBufferParameter("poolIndex"));
				if (avFrame->key_frame)
					outbuf.addBufferParameter("frameType", 0);
				else
					outbuf.addBufferParameter("frameType", 1);
				outbuf.setPts(decodeCount++ * buf.getDuration());
				outbuf.setStreamBufferNo(decodeCount);
				outbuf.setDuration(buf.getDuration());
				outputLock.lock();
				outputBuffers << outbuf;
				releaseOutputSem(0);
				outputLock.unlock();
			}
		}
	}
	return 0;
}

void FFmpegDecoder::aboutDeleteBuffer(const QMap<QString, QVariant> & pars)
{
	int ind = pars["poolIndex"].toInt();
	pool->give(poolBuffers[ind]);
}
