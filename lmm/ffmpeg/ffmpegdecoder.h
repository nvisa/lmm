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
	int getBufferCount() { return bufferCount; }
	void setBufferCount(int v) { bufferCount = v; }
	void setVideoResolution(int width, int height);
	void setH264NalChecking(bool v);
protected:
	int decode(RawBuffer buf);
	int decodeH264(const RawBuffer &lastBuf);
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
	int bufferCount;
	bool checkNalUnits;
};

#endif // FFMPEGDECODER_H
