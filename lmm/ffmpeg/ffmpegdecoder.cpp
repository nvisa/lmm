//#define __STDC_CONSTANT_MACROS
#include "ffmpegdecoder.h"

#include <lmm/debug.h>
#include <lmm/lmmcommon.h>
#include <lmm/h264parser.h>
#include <lmm/lmmbufferpool.h>

#include <stdint.h>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libswscale/swscale.h>
}

FFmpegDecoder::FFmpegDecoder(QObject *parent) :
	BaseLmmDecoder(parent)
{
	codecCtx = NULL;
	codec = NULL;
	pool = new LmmBufferPool(this);
	avFrame = NULL;
}

int FFmpegDecoder::startDecoding()
{
	codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!codec) {
		mDebug("cannot find decoder for codec %s", "AV_CODEC_ID_H264");
		return -ENOENT;
	}

	codecCtx = avcodec_alloc_context3(codec);
	/* dummy open, just for error checking */
	codecCtx->width = 1920;
	codecCtx->height = 1080;
	int err = avcodec_open2(codecCtx, codec, NULL);
	if (err) {
		mDebug("error %d opening video codec", err);
		return err;
	}
	/* close context now, it will be opened with correct resolution when needed */
	avcodec_close(codecCtx);
	avcodec_free_context(&codecCtx);

	if (avFrame) {
		av_frame_free(&avFrame);
		avFrame = NULL;
	}
	avFrame = av_frame_alloc();
	decodeCount = 0;
	detectedWidth = detectedHeight = 0;
	return 0;
}

int FFmpegDecoder::stopDecoding()
{
	avcodec_free_context(&codecCtx);
	pool->finalize();
	return 0;
}

int FFmpegDecoder::decode(RawBuffer buf)
{
	int nal = SimpleH264Parser::getNalType((const uchar *)buf.constData());
	if (detectedWidth == 0) {
		if (nal == SimpleH264Parser::NAL_SPS) {
			SimpleH264Parser::sps_t sps = SimpleH264Parser::parseSps((const uchar *)buf.constData());
			detectedWidth = 16 * (sps.pic_width_in_mbs_minus1 + 1)
					- sps.frame_crop_right_offset * 2 - sps.frame_crop_left_offset * 2;
			detectedHeight = (2 - sps.frame_mbs_only_flag) * 16 * (sps.pic_height_in_map_units_minus1 + 1)
					- sps.frame_crop_top_offset * 2 - sps.frame_crop_bottom_offset * 2;
			ffDebug() << "encountered SPS" << detectedWidth << detectedHeight;
		} else
			return 0;
	}
	mInfo("decoding %d bytes", buf.size());
	if (!codecCtx) {
		codecCtx = avcodec_alloc_context3(codec);
		codecCtx->width = detectedWidth;
		codecCtx->height = detectedHeight;
		int err = avcodec_open2(codecCtx, codec, NULL);
		if (err) {
			mDebug("error %d opening video codec", err);
			return err;
		}
	}
	if (codec->type == AVMEDIA_TYPE_VIDEO) {
		currentFrame.append((const char *)buf.constData(), buf.size());
		if (nal != SimpleH264Parser::NAL_SLICE && nal != SimpleH264Parser::NAL_SLICE_IDR)
			return 0;
		return decodeH264(buf);
	} else if (codec->type == AVMEDIA_TYPE_AUDIO) {
		int finished;
		Q_UNUSED(finished);
		int bytes = -1;//avcodec_decode_audio4(codecCtx, avFrame, &finished, packet);
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

int FFmpegDecoder::decodeH264(const RawBuffer &lastBuf)
{
	int finished;
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = (uchar *)currentFrame.constData();
	pkt.size = currentFrame.size();
	int bytes = avcodec_decode_video2(codecCtx, avFrame, &finished, &pkt);
	currentFrame.clear();
	if (bytes < 0) {
		//mDebug("error %d while decoding frame", bytes);
		return 0;
	}
	if (finished) {
		int bufsize = avpicture_get_size(codecCtx->pix_fmt, codecCtx->width, codecCtx->height);
		if (pool->freeBufferCount() == 0 && pool->usedBufferCount() == 0) {
			for (int i = 0; i < 10; i++)
				pool->addBuffer(RawBuffer("video/h-264", bufsize, this));
		}
		RawBuffer refbuf = pool->take(true);
		RawBuffer outbuf(this);
		outbuf.setRefData(refbuf.getMimeType(), refbuf.data(), refbuf.size());
		outbuf.pars()->poolIndex = refbuf.pars()->poolIndex;
		outbuf.pars()->videoWidth = codecCtx->width;
		outbuf.pars()->videoHeight = codecCtx->height;
		outbuf.pars()->avPixelFormat = codecCtx->pix_fmt;
		outbuf.pars()->streamBufferNo = lastBuf.constPars()->streamBufferNo;
		outbuf.pars()->captureTime = lastBuf.constPars()->captureTime;
		outbuf.pars()->encodeTime = lastBuf.constPars()->encodeTime;
		outbuf.pars()->pts = lastBuf.constPars()->pts;
		avpicture_layout((AVPicture *)avFrame, codecCtx->pix_fmt, codecCtx->width, codecCtx->height, (uchar *)outbuf.data(), bufsize);
		newOutputBuffer(0, outbuf);
	}
	return 0;
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
#if 0
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
#endif
}

void FFmpegDecoder::aboutToDeleteBuffer(const RawBufferParameters *params)
{
	pool->give(params->poolIndex);
}
