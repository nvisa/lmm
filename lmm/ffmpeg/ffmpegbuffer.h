#ifndef FFMPEGBUFFER_H
#define FFMPEGBUFFER_H

#include <lmm/rawbuffer.h>

struct AVPacket;

class FFmpegBufferData : public RawBufferData
{
public:
	FFmpegBufferData()
		: RawBufferData()
	{
	}
	FFmpegBufferData(const FFmpegBufferData &other)
		: RawBufferData(other)
	{
		packet = other.packet;
	}
	~FFmpegBufferData();
	AVPacket *packet;
};

class FFmpegBuffer : public RawBuffer
{
public:
	FFmpegBuffer(QString mimeType, AVPacket *packet, BaseLmmElement *parent = 0);
	AVPacket * getAVPacket();
};

#endif // FFMPEGBUFFER_H
