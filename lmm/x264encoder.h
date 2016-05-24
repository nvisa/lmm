#ifndef X264ENCODER_H
#define X264ENCODER_H

#include <lmm/baselmmelement.h>

#include <QSize>

struct x264EncoderPriv;

class x264Encoder : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit x264Encoder(QObject *parent = 0);
	virtual int start();
	virtual int stop();

	int setVideoResolution(const QSize &sz);
	int setPixelFormat(int fmt);

protected:
	virtual int processBuffer(const RawBuffer &buf);

	x264EncoderPriv *priv;
};

#endif // X264ENCODER_H
