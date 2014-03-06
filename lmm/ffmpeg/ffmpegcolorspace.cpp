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

int FFmpegColorSpace::processBuffer(const RawBuffer &buf)
{
	int w = buf.constPars()->videoWidth;
	int h = buf.constPars()->videoHeight;
	if (!swsCtx) {
		swsCtx = sws_getContext(w, h, (AVPixelFormat)inPixFmt, w, h, (AVPixelFormat)outPixFmt, SWS_BICUBIC
											, NULL, NULL, NULL);
		for (int i = 0; i < 15; i++) {
			mInfo("allocating sw scale buffer %d with size %dx%d", i, w, h);
			FFmpegBuffer buffer(mime, w, h, this);
			buffer.pars()->poolIndex = i;
			pool->addBuffer(buffer);
			poolBuffers.insert(i, buffer);
		}
	}

	mInfo("taking output buffer from buffer pool");
	RawBuffer poolbuf = pool->take();
	if (poolbuf.size() == 0)
		return -ENOENT;

	AVFrame *frame = (AVFrame *)poolbuf.constPars()->avFrame;
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

	outbuf.pars()->videoWidth = w;
	outbuf.pars()->videoHeight = h;
	outbuf.pars()->avPixelFormat = outPixFmt;
	outbuf.pars()->avFrame = (quintptr *)frame;
	outbuf.pars()->poolIndex = poolbuf.constPars()->poolIndex;
	outbuf.pars()->pts = buf.constPars()->pts;
	outbuf.pars()->streamBufferNo = buf.constPars()->streamBufferNo;
	outbuf.pars()->duration= buf.constPars()->duration;
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

void FFmpegColorSpace::aboutToDeleteBuffer(const RawBufferParameters *pars)
{
	pool->give(poolBuffers[pars->poolIndex]);
}
