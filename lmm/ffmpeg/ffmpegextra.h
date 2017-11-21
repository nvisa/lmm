#ifndef FFMPEGEXTRA_H
#define FFMPEGEXTRA_H

#include <QByteArray>

class FFmpegExtra
{
public:
	FFmpegExtra();

	static float getH264FrameRate(const QByteArray &data);
	static float getH264FrameRate(const uchar *data, int size);
};

#endif // FFMPEGEXTRA_H
