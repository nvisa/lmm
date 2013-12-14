#define __STDC_CONSTANT_MACROS
#include "ffmpegcolorspace.h"

#include <lmm/debug.h>
#include <lmm/lmmbufferpool.h>
#include <lmm/ffmpeg/ffmpegbuffer.h>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libswscale/swscale.h>
}

FFmpegColorSpace::FFmpegColorSpace(QObject *parent) :
	BaseLmmElement(parent)
{
	swsCtx = NULL;
	pool = new LmmBufferPool(this);
}

int FFmpegColorSpace::processBuffer(RawBuffer buf)
{
	int w = buf.getBufferParameter("width").toInt();
	int h = buf.getBufferParameter("height").toInt();
	if (!swsCtx) {
		swsCtx = sws_getContext(w, h, (AVPixelFormat)inPixFmt, w, h, (AVPixelFormat)outPixFmt, SWS_BICUBIC
											, NULL, NULL, NULL);
		for (int i = 0; i < 15; i++) {
			mInfo("allocating sw scale buffer %d with size %dx%d", i, w, h);
			FFmpegBuffer buffer(mime, w, h, this);
			buffer.addBufferParameter("poolIndex", i);
			pool->addBuffer(buffer);
			poolBuffers.insert(i, buffer);
		}
	}

	mInfo("taking output buffer from buffer pool");
	RawBuffer poolbuf = pool->take();
	if (poolbuf.size() == 0)
		return -ENOENT;

	AVFrame *frame = (AVFrame *)poolbuf.getBufferParameter("AVFrame").toULongLong();
	RawBuffer outbuf;

	int stride = w;
	if (inPixFmt == AV_PIX_FMT_RGB24)
		stride = w * 3;
	int srcStride[3] = { stride, stride, stride };
	const uchar *in = (const uchar *)buf.constData();
	const uchar *src[] = { in , in, in };
	sws_scale(swsCtx, src, srcStride, 0, h,
			  frame->data, frame->linesize);

	outbuf = RawBuffer(poolbuf.getMimeType(),
					 (const void *)poolbuf.data(), poolbuf.size(), this);

	outbuf.addBufferParameter("width", w);
	outbuf.addBufferParameter("height", h);
	outbuf.addBufferParameter("avPixelFmt", outPixFmt);
	outbuf.addBufferParameter("avFrame", (quintptr)frame);
	outbuf.addBufferParameter("poolIndex", poolbuf.getBufferParameter("poolIndex"));
	outbuf.setPts(buf.getPts());
	outbuf.setStreamBufferNo(buf.streamBufferNo());
	outbuf.setDuration(buf.getDuration());
	newOutputBuffer(0, outbuf);

	return 0;
}

int FFmpegColorSpace::convertToRgb()
{
	inPixFmt = AV_PIX_FMT_GRAY8;
	outPixFmt = AV_PIX_FMT_RGB24;
	mime = "video/x-raw-rgb";
	return 0;
}

int FFmpegColorSpace::convertToGray()
{
	outPixFmt = AV_PIX_FMT_GRAY8;
	inPixFmt = AV_PIX_FMT_RGB24;
	mime = "video/x-raw-gray";
	return 0;
}

void FFmpegColorSpace::aboutDeleteBuffer(const QHash<QString, QVariant> &pars)
{
	int ind = pars["poolIndex"].toInt();
	pool->give(poolBuffers[ind]);
}
