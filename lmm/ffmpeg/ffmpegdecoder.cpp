#define __STDC_CONSTANT_MACROS
#include "ffmpegdecoder.h"

#include <lmm/ffmpeg/ffmpegbuffer.h>
#include <lmm/lmmbufferpool.h>
#include <lmm/debug.h>
#include <lmm/lmmcommon.h>

#include <stdint.h>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libswscale/swscale.h>
}

FFmpegDecoder::FFmpegDecoder(QObject *parent) :
	BaseLmmDecoder(parent)
{
	rgbOut = true;
	codecCtx = NULL;
	outWidth = 0;
	outHeight = 0;
	codec = NULL;
	onlyKeyframe = false;
	pool = new LmmBufferPool(this);
	avFrame = NULL;
	convert = true;
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
	if (codecCtx) {
		codecCtx = NULL;
		sws_freeContext(swsCtx);
	}
	if (avFrame) {
		avcodec_free_frame(&avFrame);
		avFrame = NULL;
	}
	avFrame = avcodec_alloc_frame();
	swsCtx = NULL;
	decodeCount = 0;
	return 0;
}

int FFmpegDecoder::stopDecoding()
{
	avcodec_close(codecCtx);
	poolBuffers.clear();
	pool->finalize();
	return 0;
}

int FFmpegDecoder::decode(RawBuffer buf)
{
	mInfo("decoding %d bytes", buf.size());
	FFmpegBuffer *ffbuf = (FFmpegBuffer *)&buf;
	AVPacket *packet = ffbuf->getAVPacket();
	if (onlyKeyframe && (packet->flags & AV_PKT_FLAG_KEY) == 0)
		return 0;
	if (codec->type == AVMEDIA_TYPE_VIDEO) {
		int finished;
		int bytes = avcodec_decode_video2(codecCtx, avFrame, &finished, packet);
		if (bytes < 0) {
			mDebug("error %d while decoding frame", bytes);
			return 0;
		}
		if (finished) {
			if (convert)
				return convertColorSpace(buf);
		}
		return 0;
	} else if (codec->type == AVMEDIA_TYPE_AUDIO) {
		int finished;
		int bytes = avcodec_decode_audio4(codecCtx, avFrame, &finished, packet);
		if (bytes < 0) {
			mDebug("error %d while decoding audio frame", bytes);
			return 0;
		}
		/* since we are using demuxer all packet should be used by the decoder */
		int frameSize = av_samples_get_buffer_size(NULL, codecCtx->channels, avFrame->nb_samples, codecCtx->sample_fmt, 1);
		RawBuffer outbuf(QString("audio/x-raw-int,rate=%1,fmt=%2,channels=%3").arg(codecCtx->sample_rate)
					  .arg(codecCtx->sample_fmt).arg(codecCtx->channels),
					  avFrame->data[0], frameSize);
		outbuf.pars()->pts = buf.constPars()->pts;
		outbuf.pars()->streamBufferNo = buf.constPars()->streamBufferNo;
		outbuf.pars()->duration = buf.constPars()->duration;
		return newOutputBuffer(0, outbuf);
	}

	return -EINVAL;
}

int FFmpegDecoder::convertColorSpace(RawBuffer buf)
{
	if (codecCtx->pix_fmt == PIX_FMT_YUV420P || codecCtx->pix_fmt == PIX_FMT_YUVJ420P) {
		mInfo("decoded video frame");
		if (!swsCtx) {
			mDebug("getting sw scale context");
			if (!outWidth || !outHeight) {
				outWidth = codecCtx->width;
				outHeight = codecCtx->height;
				if (keepAspectRatio)
					outWidth = codecCtx->width * outHeight / codecCtx->height;
				else
					outWidth = codecCtx->width;
			}
			if (rgbOut)
				swsCtx = sws_getContext(codecCtx->width, codecCtx->height, static_cast<PixelFormat>(codecCtx->pix_fmt),
									outWidth, outHeight, AV_PIX_FMT_RGB24, SWS_BICUBIC
									, NULL, NULL, NULL);
			else
				swsCtx = sws_getContext(codecCtx->width, codecCtx->height, static_cast<PixelFormat>(codecCtx->pix_fmt),
									outWidth, outHeight, AV_PIX_FMT_GRAY8, SWS_BICUBIC
									, NULL, NULL, NULL);
			QString mime = "video/x-raw-rgb";
			if (!rgbOut)
				mime = "video/x-raw-gray";
			for (int i = 0; i < 15; i++) {
				mInfo("allocating sw scale buffer %d with size %dx%d", i, outWidth, outHeight);
				FFmpegBuffer buf(mime, outWidth, outHeight, this);
				buf.pars()->poolIndex = i;
				pool->addBuffer(buf);
				poolBuffers.insert(i, buf);
			}
		}
		mInfo("taking output buffer from buffer pool");
		RawBuffer poolbuf = pool->take();
		if (poolbuf.size() == 0)
			return -ENOENT;
		AVFrame *frame = (AVFrame *)poolbuf.constPars()->avFrame;
		RawBuffer outbuf;
		sws_scale(swsCtx, avFrame->data, avFrame->linesize, 0, codecCtx->height,
				  frame->data, frame->linesize);
		if (rgbOut) {
			outbuf = RawBuffer(QString("video/x-raw-rgb"),
							 (const void *)poolbuf.data(), poolbuf.size(), this);
		} else {
			/*int lsz = avFrame->linesize[0];
			for (int i = 0; i < codecCtx->height; i++)
				memcpy(frame->data[0] + i * codecCtx->width, avFrame->data[0] + i * lsz, codecCtx->width);*/
			outbuf = RawBuffer(QString("video/x-raw-gray"), (const void *)poolbuf.data(), poolbuf.size(), this);
		}
		outbuf.pars()->videoWidth = outWidth;
		outbuf.pars()->videoHeight = outHeight;
		if (rgbOut)
			outbuf.pars()->avPixelFormat = AV_PIX_FMT_RGB24;
		else
			outbuf.pars()->avPixelFormat = AV_PIX_FMT_GRAY8;
		outbuf.pars()->avFrame = (quintptr *)frame;
		outbuf.pars()->poolIndex = poolbuf.constPars()->poolIndex;
		if (avFrame->key_frame)
			outbuf.pars()->frameType = 0;
		else
			outbuf.pars()->frameType = 1;
		outbuf.pars()->pts = buf.constPars()->pts;
		outbuf.pars()->streamBufferNo = buf.constPars()->streamBufferNo;
		outbuf.pars()->duration = buf.constPars()->duration;
		return newOutputBuffer(0, outbuf);
	}
	return -EINVAL;
}

#define IS_INTERLACED(a) ((a)&MB_TYPE_INTERLACED)
#define IS_16X16(a)      ((a)&MB_TYPE_16x16)
#define IS_16X8(a)       ((a)&MB_TYPE_16x8)
#define IS_8X16(a)       ((a)&MB_TYPE_8x16)
#define IS_8X8(a)        ((a)&MB_TYPE_8x8)
#define USES_LIST(a, list) ((a) & ((MB_TYPE_P0L0|MB_TYPE_P1L0)<<(2*(list))))

void FFmpegDecoder::print_vector(int x, int y, int dx, int dy)
{
	/*int w = codecCtx->width;
	int h = codecCtx->height;
	int sum = 0;
	if (dx != 10000 && dy != 10000){
		sum = sum + sqrt((double)dx*(double)dx/w/w + (double)dy*(double)dy/h/h);
	}*/
	mDebug("%d %d ; %d %d", x, y, dx, dy);
}

void FFmpegDecoder::printMotionVectors(AVFrame *pict)
{
	AVCodecContext *ctx = codecCtx;
	const int mb_width  = (ctx->width + 15) / 16;
	const int mb_height = (ctx->height + 15) / 16;
	const int mb_stride = mb_width + 1;
	const int mv_sample_log2 = 4 - pict->motion_subsample_log2;
	const int mv_stride = (mb_width << mv_sample_log2) + (ctx->codec_id == AV_CODEC_ID_H264 ? 0 : 1);
	const int quarter_sample = (ctx->flags & CODEC_FLAG_QPEL) != 0;
	const int shift = 1 + quarter_sample;


	mDebug("frame %d, %d x %d", 0, mb_height, mb_width);

	for (int mb_y = 0; mb_y < mb_height; mb_y++) {
		for (int mb_x = 0; mb_x < mb_width; mb_x++) {
			const int mb_index = mb_x + mb_y * mb_stride;
			if (!pict->motion_val) {
				mDebug("no motion vectors");
				continue;
			}
			for (int type = 0; type < 3; type++) {
				int direction = 0;
				switch (type) {
				case 0:
					if (pict->pict_type != AV_PICTURE_TYPE_P)
						continue;
					direction = 0;
					break;
				case 1:
					if (pict->pict_type != AV_PICTURE_TYPE_B)
						continue;
					direction = 0;
					break;
				case 2:
					if (pict->pict_type != AV_PICTURE_TYPE_B)
						continue;
					direction = 1;
					break;
				}
#if 1
				if (!USES_LIST(pict->mb_type[mb_index], direction)) {
					mDebug("not using list");
#define NO_MV 10000
					if (IS_8X8(pict->mb_type[mb_index])) {
						print_vector(mb_x, mb_y, NO_MV, NO_MV);
						print_vector(mb_x, mb_y, NO_MV, NO_MV);
						print_vector(mb_x, mb_y, NO_MV, NO_MV);
						print_vector(mb_x, mb_y, NO_MV, NO_MV);
					} else if (IS_16X8(pict->mb_type[mb_index])) {
						print_vector(mb_x, mb_y, NO_MV, NO_MV);
						print_vector(mb_x, mb_y, NO_MV, NO_MV);
					} else if (IS_8X16(pict->mb_type[mb_index])) {
						print_vector(mb_x, mb_y, NO_MV, NO_MV);
						print_vector(mb_x, mb_y, NO_MV, NO_MV);
					} else {
						print_vector(mb_x, mb_y, NO_MV, NO_MV);
					}
#undef NO_MV
					continue;
				}

				if (IS_8X8(pict->mb_type[mb_index])) {
					mDebug("8x8");
					for (int i = 0; i < 4; i++) {
						int xy = (mb_x*2 + (i&1) + (mb_y*2 + (i>>1))*mv_stride) << (mv_sample_log2-1);
						int dx = (pict->motion_val[direction][xy][0]>>shift);
						int dy = (pict->motion_val[direction][xy][1]>>shift);
						print_vector(mb_x, mb_y, dx, dy);
					}
				} else if (IS_16X8(pict->mb_type[mb_index])) {
					mDebug("16x8");
					for (int i = 0; i < 2; i++) {
						int xy = (mb_x*2 + (mb_y*2 + i)*mv_stride) << (mv_sample_log2-1);
						int dx = (pict->motion_val[direction][xy][0]>>shift);
						int dy = (pict->motion_val[direction][xy][1]>>shift);

						if (IS_INTERLACED(pict->mb_type[mb_index]))
							dy *= 2;

						print_vector(mb_x, mb_y, dx, dy);
					}
				} else if (IS_8X16(pict->mb_type[mb_index])) {
					mDebug("8x16");
					for (int i = 0; i < 2; i++) {
						int xy =  (mb_x*2 + i + mb_y*2*mv_stride) << (mv_sample_log2-1);
						int dx = (pict->motion_val[direction][xy][0]>>shift);
						int dy = (pict->motion_val[direction][xy][1]>>shift);

						if (IS_INTERLACED(pict->mb_type[mb_index]))
							dy *= 2;

						print_vector(mb_x, mb_y, dx, dy);
					}
				} else {
#endif
					mDebug("none");
					int xy = (mb_x + mb_y*mv_stride) << mv_sample_log2;
					int dx = (pict->motion_val[direction][xy][0]>>shift);
					int dy = (pict->motion_val[direction][xy][1]>>shift);
					print_vector(mb_x, mb_y, dx, dy);
				}
			}
			//printf("--\n");
		}
		//printf("====\n");
	}
}

void FFmpegDecoder::aboutToDeleteBuffer(const RawBufferParameters *params)
{
	int ind = params->poolIndex;
	pool->give(poolBuffers[ind]);
}
