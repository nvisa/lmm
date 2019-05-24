#ifndef TX1JPEGENCODER_H
#define TX1JPEGENCODER_H

#include <lmm/baselmmelement.h>

class TX1JpegEncoderPriv;

class TX1JpegEncoder : public BaseLmmElement
{
	Q_OBJECT
public:
	TX1JpegEncoder(QObject *parent = 0);
	~TX1JpegEncoder();
	void setQuality(int q);
	int getQuality();

protected:
	int processBuffer(const RawBuffer &buf);

	TX1JpegEncoderPriv *priv;
};

#endif // TX1JPEGENCODER_H
