
#ifndef FFCOMPAT_H
#define FFCOMPAT_H

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
}

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(52, 102, 0)
#define avio_open url_fopen
#define avio_close url_fclose
static void avformat_free_context(AVFormatContext *c)
{
	delete c;
}

#endif

#ifndef CODEC_TYPE_VIDEO
#define CODEC_TYPE_VIDEO AVMEDIA_TYPE_VIDEO
#endif

#ifndef CODEC_TYPE_AUDIO
#define CODEC_TYPE_AUDIO AVMEDIA_TYPE_AUDIO
#endif

#ifndef AV_PIX_FMT_GRAY8
#define AV_PIX_FMT_GRAY8 AV_PIX_FMT_GRAY8
#endif

#ifndef URL_RDONLY
#define dump_format av_dump_format
#define av_write_header(_x) avformat_write_header(_x, NULL)
#define URL_WRONLY 0
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 45, 101)
	#define FF_INPUT_BUFFER_PADDING_SIZE AV_INPUT_BUFFER_PADDING_SIZE
	#define CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
#elif LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 25, 0)
	#define av_frame_alloc avcodec_alloc_frame
#endif

#endif // FFCOMPAT_H
