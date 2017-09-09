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

class BaseVideoScaler
{
public:
	BaseVideoScaler() {}
	virtual int convert(SwsContext *ctx, const uchar *data, int w, int h, uchar *out) = 0;
	virtual ~BaseVideoScaler() {}
};

class ScalerJPEGYUV420P : public BaseVideoScaler
{
public:
	ScalerJPEGYUV420P() {}
	int convert(SwsContext *ctx, const uchar *data, int w, int h, uchar *out)
	{
		int stride = w;

		/* src info */
		int srcStride[3] = { stride, stride / 2, stride / 2};
		const uchar *Y = data;
		const uchar *U = data + stride * h;
		const uchar *V = data + stride * h * 5 / 4;
		const uchar *src[3] = { Y , U, V };

		/* dst info */
		int dstW = w;
		int dstH = h;
		int dstStride[3] = { dstW, dstW / 2, dstW / 2};
		uchar * outData[3] = { out, out + dstW * dstH, out + dstW * dstH * 5 / 4};
		sws_scale(ctx, src, srcStride, 0, h, outData, dstStride);
		return 0;
	}
};

class ScalerGenericRGB : public BaseVideoScaler
{
public:
	ScalerGenericRGB() {}
	int convert(SwsContext *ctx, const uchar *data, int w, int h, uchar *out)
	{
		int stride = w;

		/* src info */
		int srcStride[3] = { stride, stride / 2, stride / 2};
		const uchar *Y = data;
		const uchar *U = data + stride * h;
		const uchar *V = data + stride * h * 5 / 4;
		const uchar *src[3] = { Y , U, V };

		/* src info */
		/*if (pixFmt == AV_PIX_FMT_NV12) {
			srcStride[0] = stride;
			srcStride[1] = stride;
			const uchar *Y = data;
			const uchar *UV = data + stride * p->h;
			src[0] = Y;
			src[1] = UV;
		}*/

		/* dst info */
		int dstStride[1] = { w * 3};
		uchar * outData[1] = { out };
		sws_scale(ctx, src, srcStride, 0, h,
				  outData, dstStride);
		return 0;
	}
};

class ScalerNV12RGB32 : public BaseVideoScaler
{
public:
	ScalerNV12RGB32() {}

	int convert(SwsContext *ctx, const uchar *data, int w, int h, uchar *out)
	{
		int stride = w;

		int srcStride[2] = { stride, stride };
		const uchar *Y = data;
		const uchar *UV = data + stride * h;
		const uchar *src[2] = { Y , UV };
		int dstStride[1] = { w * 4};
		uchar * outData[1] = { out };
		sws_scale(ctx, src, srcStride, 0, h,
				  outData, dstStride);
		return 0;
	}
};

class ScalerRGB32YUV420P : public BaseVideoScaler
{
public:
	ScalerRGB32YUV420P() {}

	int convert(SwsContext *ctx, const uchar *data, int w, int h, uchar *out)
	{
		int stride = w;

		/* src info */
		int dstStride[3] = { stride, stride / 2, stride / 2};
		uchar *Y = out;
		uchar *U = out + stride * h;
		uchar *V = out + stride * h * 5 / 4;
		uchar *dst[3] = { Y , U, V };

		/* dst info */
		int srcStride[1] = { w * 4 };
		const uchar * src[4] = { data, data, data, data };
		sws_scale(ctx, src, srcStride, 0, h, dst, dstStride);

		/*qDebug() << "check";
		qDebug() << ((uintptr_t)dst[0]&15);
		qDebug() << ((uintptr_t)dst[1]&15);
		qDebug() << ((uintptr_t)dst[2]&15);
		qDebug() << ((uintptr_t)src[0]&15);
		qDebug() << ((uintptr_t)src[1]&15);
		qDebug() << ((uintptr_t)src[2]&15);*/

		return 0;
	}
};

FFmpegColorSpace::FFmpegColorSpace(QObject *parent) :
	BaseLmmElement(parent)
{
	swsCtx = NULL;
	pool = new LmmBufferPool(this);
	scaler = NULL;
}

int FFmpegColorSpace::processBuffer(const RawBuffer &buf)
{
	int w = buf.constPars()->videoWidth;
	int h = buf.constPars()->videoHeight;
	if (!swsCtx) {
		if (buf.constPars()->avPixelFormat != -1)
			inPixFmt = buf.constPars()->avPixelFormat;
		int bufsize = avpicture_get_size((AVPixelFormat)outPixFmt, w, h);
		swsCtx = sws_getContext(w, h, (AVPixelFormat)inPixFmt, w, h, (AVPixelFormat)outPixFmt, SWS_BICUBIC
											, NULL, NULL, NULL);
		for (int i = 0; i < 15; i++) {
			mInfo("allocating sw scale buffer %d with size %dx%d", i, w, h);
			RawBuffer buffer(mime, bufsize);
			pool->addBuffer(buffer);
		}

		if (outPixFmt == AV_PIX_FMT_RGB24)
			mime = "video/x-raw-rgb";
		else if (outPixFmt == AV_PIX_FMT_GRAY8)
			mime = "video/x-raw-gray";
		mDebug("inpix=%d outpix=%d mime=%s", inPixFmt, outPixFmt, qPrintable(mime));

		if (inPixFmt == AV_PIX_FMT_NV12
				&& outPixFmt == AV_PIX_FMT_RGB24)
			scaler = new ScalerNV12RGB32;
		else if (inPixFmt == AV_PIX_FMT_YUV420P
				&& outPixFmt == AV_PIX_FMT_RGB24)
			scaler = new ScalerGenericRGB;
		else if (outPixFmt == AV_PIX_FMT_RGB24)
			scaler = new ScalerGenericRGB;
		else if (inPixFmt == AV_PIX_FMT_RGBA
				 && outPixFmt == AV_PIX_FMT_YUV420P)
			scaler = new ScalerRGB32YUV420P;

		if (!scaler)
			return -ENOENT;
	}

	mInfo("taking output buffer from buffer pool");
	RawBuffer poolbuf = pool->take();
	RawBuffer outbuf(this);
	outbuf.setRefData(mime, poolbuf.data(), poolbuf.size());
	mInfo("Converting using scaler %p", scaler);
	scaler->convert(swsCtx, (const uchar *)buf.constData(), w, h, (uchar *)outbuf.data());

	outbuf.pars()->videoWidth = w;
	outbuf.pars()->videoHeight = h;
	outbuf.pars()->avPixelFormat = outPixFmt;
	outbuf.pars()->poolIndex = poolbuf.constPars()->poolIndex;
	outbuf.pars()->pts = buf.constPars()->pts;
	outbuf.pars()->streamBufferNo = buf.constPars()->streamBufferNo;
	outbuf.pars()->duration = buf.constPars()->duration;
	outbuf.pars()->captureTime = buf.constPars()->captureTime;
	outbuf.pars()->encodeTime = buf.constPars()->encodeTime;
	return newOutputBuffer(0, outbuf);
}

int FFmpegColorSpace::setOutputFormat(int outfmt)
{
	outPixFmt = outfmt;
	return 0;
}

int FFmpegColorSpace::setInputFormat(int infmt)
{
	inPixFmt = infmt;
	return 0;
}

void FFmpegColorSpace::aboutToDeleteBuffer(const RawBufferParameters *pars)
{
	pool->give(pars->poolIndex);
}
