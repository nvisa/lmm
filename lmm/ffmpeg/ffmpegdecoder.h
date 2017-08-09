#ifndef FFMPEGDECODER_H
#define FFMPEGDECODER_H

#include <lmm/baselmmdecoder.h>

#include <QMap>

class LmmBufferPool;
struct AVStream;
struct AVCodec;
struct AVCodecContext;
struct AVFrame;

class FFmpegDecoder : public BaseLmmDecoder
{
	Q_OBJECT

public:
	explicit FFmpegDecoder(QObject *parent = 0);
	virtual int startDecoding();
	virtual int stopDecoding();
	virtual void aboutToDeleteBuffer(const RawBufferParameters *params);
protected:
	int decode(RawBuffer buf);
	int decodeH264();
	void printMotionVectors(AVFrame *pict);
	void print_vector(int x, int y, int dx, int dy);
private:
	AVCodec *codec;
	AVCodecContext* codecCtx;
	AVFrame *avFrame;
	int decodeCount;
	LmmBufferPool *pool;
	int detectedWidth;
	int detectedHeight;
	QByteArray currentFrame;
};

#endif // FFMPEGDECODER_H
