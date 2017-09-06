#include "ffmpegextra.h"

#include <lmm/debug.h>

extern "C" {
	float get_h264_frame_rate(const uint8_t *data, int size);
}

FFmpegExtra::FFmpegExtra()
{

}

float FFmpegExtra::getH264FrameRate(const QByteArray &data)
{
	return get_h264_frame_rate((const uchar *)data.constData(), data.size());
}

float FFmpegExtra::getH264FrameRate(const uchar *data, int size)
{
	return get_h264_frame_rate(data, size);
}

