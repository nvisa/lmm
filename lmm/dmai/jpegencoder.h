#ifndef JPEGENCODER_H
#define JPEGENCODER_H

#include "dmaiencoder.h"

#include <ti/sdo/dmai/ce/Ienc1.h>

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

	virtual int startCodec();
	virtual int stopCodec();
	virtual int encode(Buffer_Handle buffer, const RawBuffer source);
};

#endif // JPEGENCODER_H
