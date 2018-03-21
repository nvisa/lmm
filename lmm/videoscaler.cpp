#include "videoscaler.h"
#include "ffmpeg/ffmpegcolorspace.h" //for format name conversion

#include <lmm/debug.h>
#include <lmm/lmmbufferpool.h>

extern "C" {
	#include <libavformat/avformat.h>
}

#ifdef HAVE_LIBYUV
#include <libyuv.h>

static const int kTileX = 32;
static const int kTileY = 32;

static int TileARGBScale(const uint8* src_argb, int src_stride_argb,
						 int src_width, int src_height,
						 uint8* dst_argb, int dst_stride_argb,
						 int dst_width, int dst_height,
						 libyuv::FilterMode filtering) {
	for (int y = 0; y < dst_height; y += kTileY) {
		for (int x = 0; x < dst_width; x += kTileX) {
			int clip_width = kTileX;
			if (x + clip_width > dst_width) {
				clip_width = dst_width - x;
			}
			int clip_height = kTileY;
			if (y + clip_height > dst_height) {
				clip_height = dst_height - y;
			}
			int r = libyuv::ARGBScaleClip(src_argb, src_stride_argb,
										  src_width, src_height,
										  dst_argb, dst_stride_argb,
										  dst_width, dst_height,
										  x, y, clip_width, clip_height, filtering);
			if (r) {
				return r;
			}
		}
	}
	return 0;
}

#endif

VideoScaler::VideoScaler(QObject *parent)
	: BaseLmmElement(parent)
{
	pool = new LmmBufferPool(this);
	bufferCount = 15;
	setOutputResolution(0, 0);
	mode = 0;
}

void VideoScaler::setOutputResolution(int width, int height)
{
	dstW = width;
	dstH = height;
}

int VideoScaler::processBuffer(const RawBuffer &buf)
{
	if (mode == 1)
		return processConverter(buf);
	return processScaler(buf);
}

int VideoScaler::processScaler(const RawBuffer &buf)
{
	int w = buf.constPars()->videoWidth;
	int h = buf.constPars()->videoHeight;

	if (!dstW)
		dstW = w / 2;
	if (!dstH)
		dstH = h / 2;

	int bufsize = (unsigned long long)buf.size() * dstW * dstH / w / h;
	if (pool->freeBufferCount() == 0 && pool->usedBufferCount() == 0) {
		mime = "video/x-raw-yuv";
		if (buf.constPars()->avPixelFormat == AV_PIX_FMT_ARGB)
			mime = "video/x-raw-rgb";
		for (int i = 0; i < bufferCount; i++) {
			mInfo("allocating sw scale buffer %d with size %dx%d", i, dstW, dstH);
			RawBuffer buffer(mime, bufsize);
			pool->addBuffer(buffer);
		}
	}

	mInfo("taking output buffer from buffer pool");
	RawBuffer poolbuf = pool->take();
	RawBuffer outbuf(this);
	outbuf.setRefData(mime, poolbuf.data(), poolbuf.size());

#ifdef HAVE_LIBYUV
	if (buf.constPars()->avPixelFormat == AV_PIX_FMT_ARGB)
		libyuv::ARGBScale((const uchar *)buf.constData(), w * 4, w, h, (uchar *)outbuf.data(), dstW * 4, dstW, dstH, libyuv::kFilterNone);
	else if (buf.constPars()->avPixelFormat == AV_PIX_FMT_YUV420P ||
			 buf.constPars()->avPixelFormat == AV_PIX_FMT_YUVJ420P) {
		const uchar *Y = (const uchar *)buf.constData();
		const uchar *U = Y + w * h;
		const uchar *V = Y + w * h * 5 / 4;

		uchar *Yd = (uchar *)outbuf.data();
		uchar *Ud = Yd + w * h;
		uchar *Vd = Yd + w * h * 5 / 4;

		libyuv::I420Scale(Y, w, U, w / 2, V, w / 2, w, h, Yd, dstW, Ud, dstW / 2, Vd, dstW / 2, dstW, dstH, libyuv::kFilterNone);
	} else
		mDebug("Un-supported input color-space format: %s",
			   qPrintable(FFmpegColorSpace::getName(buf.constPars()->avPixelFormat)));
	//TileARGBScale((const uchar *)buf.constData(), w * 4, w, h, (uchar *)outbuf.data(), dstW * 4, dstW, dstH, libyuv::kFilterNone);
#endif

	outbuf.pars()->videoWidth = dstW;
	outbuf.pars()->videoHeight = dstH;
	outbuf.pars()->avPixelFormat = buf.constPars()->avPixelFormat;
	outbuf.pars()->poolIndex = poolbuf.constPars()->poolIndex;
	outbuf.pars()->pts = buf.constPars()->pts;
	outbuf.pars()->streamBufferNo = buf.constPars()->streamBufferNo;
	outbuf.pars()->duration = buf.constPars()->duration;
	outbuf.pars()->captureTime = buf.constPars()->captureTime;
	outbuf.pars()->encodeTime = buf.constPars()->encodeTime;
	outbuf.pars()->metaData = buf.constPars()->metaData;
	return newOutputBuffer(0, outbuf);
}

int VideoScaler::processConverter(const RawBuffer &buf)
{
	int w = buf.constPars()->videoWidth;
	int h = buf.constPars()->videoHeight;

	if (!dstW)
		dstW = w;
	if (!dstH)
		dstH = h;

	int bufsize = dstW * dstH * 4;
	if (pool->freeBufferCount() == 0 && pool->usedBufferCount() == 0) {
		mime = "video/x-raw-rgb";
		for (int i = 0; i < bufferCount; i++) {
			mInfo("allocating sw scale buffer %d with size %dx%d", i, dstW, dstH);
			RawBuffer buffer(mime, bufsize);
			pool->addBuffer(buffer);
		}
	}

	mInfo("taking output buffer from buffer pool");
	RawBuffer poolbuf = pool->take();
	RawBuffer outbuf(this);
	outbuf.setRefData(mime, poolbuf.data(), poolbuf.size());

	const uchar *Y = (const uchar *)buf.constData();
	const uchar *U = Y + w * h;
	const uchar *V = Y + w * h * 5 / 4;

#ifdef HAVE_LIBYUV
	libyuv::I420ToARGB(Y, w, U, w / 2, V, w / 2, (uchar *)outbuf.data(), w * 4, w, h);
#endif

	outbuf.pars()->videoWidth = dstW;
	outbuf.pars()->videoHeight = dstH;
	outbuf.pars()->avPixelFormat = AV_PIX_FMT_ARGB;
	outbuf.pars()->poolIndex = poolbuf.constPars()->poolIndex;
	outbuf.pars()->pts = buf.constPars()->pts;
	outbuf.pars()->streamBufferNo = buf.constPars()->streamBufferNo;
	outbuf.pars()->duration = buf.constPars()->duration;
	outbuf.pars()->captureTime = buf.constPars()->captureTime;
	outbuf.pars()->encodeTime = buf.constPars()->encodeTime;
	outbuf.pars()->metaData = buf.constPars()->metaData;
	return newOutputBuffer(0, outbuf);
}

void VideoScaler::aboutToDeleteBuffer(const RawBufferParameters *params)
{
	pool->give(params->poolIndex);
}

