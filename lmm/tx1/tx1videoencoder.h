#ifndef TX1VIDEOENCODER_H
#define TX1VIDEOENCODER_H

#include <lmm/baselmmelement.h>

class NvPriv;

class TX1VideoEncoder : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit TX1VideoEncoder(QObject *parent = 0);

	virtual int start();
	virtual int stop();
	int processBuffer(const RawBuffer &buf);
	void setBitrate(int bitrate);
	void setFps(float fps);

	static void encodedFrameReady(TX1VideoEncoder *enc, unsigned char *data, uint32_t length);

public slots:
protected:
	void processEncodedFrame(unsigned char *data, uint32_t length);

	NvPriv *priv;
};

#endif // TX1VIDEOENCODER_H
