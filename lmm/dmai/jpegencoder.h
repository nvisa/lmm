#ifndef JPEGENCODER_H
#define JPEGENCODER_H

#include <lmm/dmai/dmaiencoder.h>

#include <ti/sdo/dmai/ce/Ienc1.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/BufferGfx.h>

class JpegEncoder : public DmaiEncoder
{
	Q_OBJECT
public:
	explicit JpegEncoder(QObject *parent = 0);
	void setQualityFactor(int q);
signals:
	
public slots:
private:
	IMGENC1_DynamicParams   defaultDynParams;
	Ienc1_Handle hCodec;
	int qFact;
	BufTab_Handle outputBufTab;

	virtual int startCodec(bool alloc = true);
	virtual int stopCodec();
	virtual int encode(Buffer_Handle buffer, const RawBuffer source);
};

#endif // JPEGENCODER_H
