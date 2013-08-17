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
	virtual int decode();
	virtual void aboutDeleteBuffer(const QMap<QString, QVariant> &pars);

private:
	AVCodec *codec;
	AVCodecContext* codecCtx;
	AVFrame *avFrame;
	AVFrame *rgbFrame;
	SwsContext *swsCtx;
	int decodeCount;
	LmmBufferPool *pool;
	QMap<int, RawBuffer> poolBuffers;
};

#endif // FFMPEGDECODER_H
