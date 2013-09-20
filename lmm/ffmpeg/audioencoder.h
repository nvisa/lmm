#ifndef AUDIOENCODER_H
#define AUDIOENCODER_H

#include <lmm/baselmmelement.h>

struct AVCodec;
struct AVCodecContext;

class AudioEncoder : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit AudioEncoder(QObject *parent = 0);
	virtual int start();
	virtual int stop();
	AVCodecContext * getCodecContext() { return c; }
protected:
	virtual int startCodec();
	virtual int stopCodec();
	virtual int processBuffer(RawBuffer buf);
	int encode(const short *samples);

	AVCodec *codec;
	AVCodecContext *c;
	uchar *outbuf;
	int outSize;
	RawBuffer sbuf;
	int avail;
};

#endif // AUDIOENCODER_H
