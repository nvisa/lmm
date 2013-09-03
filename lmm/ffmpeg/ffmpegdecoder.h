#ifndef FFMPEGDECODER_H
#define FFMPEGDECODER_H

#include <lmm/baselmmdecoder.h>

#include <QMap>

class LmmBufferPool;
struct AVStream;
struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;

class FFmpegDecoder : public BaseLmmDecoder
{
	Q_OBJECT

public:
	explicit FFmpegDecoder(QObject *parent = 0);
	AVCodecContext * getStream() { return codecCtx; }
	virtual int setStream(AVCodecContext *stream);
	virtual int startDecoding();
	virtual int stopDecoding();
	virtual void aboutDeleteBuffer(const QMap<QString, QVariant> &pars);
	void setRgbOutput(bool val) { rgbOut = val; }
protected:
	int decode(RawBuffer buf);
	void printMotionVectors(AVFrame *pict);
	void print_vector(int x, int y, int dx, int dy);
private:
	AVCodec *codec;
	AVCodecContext* codecCtx;
	AVFrame *avFrame;
	AVFrame *rgbFrame;
	SwsContext *swsCtx;
	int decodeCount;
	LmmBufferPool *pool;
	QMap<int, RawBuffer> poolBuffers;
	bool rgbOut;
};

#endif // FFMPEGDECODER_H
