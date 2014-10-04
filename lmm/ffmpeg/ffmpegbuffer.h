#ifndef FFMPEGBUFFER_H
#define FFMPEGBUFFER_H

#include <lmm/rawbuffer.h>

struct AVPacket;
struct AVFrame;

class FFmpegBufferData : public RawBufferData
{
public:
	FFmpegBufferData()
		: RawBufferData()
	{
		frameData = NULL;
	}
	FFmpegBufferData(const FFmpegBufferData &other)
		: RawBufferData(other)
	{
		packet = other.packet;
		frame = other.frame;
	}
	~FFmpegBufferData();
	AVPacket *packet;
	AVFrame *frame;
	uchar *frameData;
	int frameSize;
};

class FFmpegBuffer : public RawBuffer
{
public:
	FFmpegBuffer(QString mimeType, AVPacket *packet, BaseLmmElement *parent = 0);
	FFmpegBuffer(QString mimeType, int width, int height, BaseLmmElement *parent = 0);
	AVPacket * getAVPacket();
	AVFrame * getAVFrame();
};

#endif // FFMPEGBUFFER_H
