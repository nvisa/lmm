#define __STDC_CONSTANT_MACROS
#include "rtpmjpegmux.h"

extern "C" {
	#include <libavformat/avformat.h>
}

#ifdef CodecType
#undef CodecType
#endif

RtpMjpegMux::RtpMjpegMux(QObject *parent) :
	RtpMux(parent)
{
	/* ffmpeg needs help for detecting mjpeg format */
	inputFmt = av_find_input_format("mjpeg");
}

Lmm::CodecType RtpMjpegMux::codecType()
{
	return Lmm::CODEC_JPEG;
}
