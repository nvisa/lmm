#ifndef JPEGENCODER_H
#define JPEGENCODER_H

#include "dmaiencoder.h"

#include <ti/sdo/dmai/ce/Ienc1.h>

class JpegEncoder : public DmaiEncoder
{
	Q_OBJECT
public:
	explicit JpegEncoder(QObject *parent = 0);
signals:
	
public slots:
private:
	int encodeCount;
	int imageWidth;
	int imageHeight;
	IMGENC1_DynamicParams   defaultDynParams;
	Ienc1_Handle hCodec;

	virtual int startCodec();
	virtual int stopCodec();
	virtual int encode(Buffer_Handle buffer);
};

#endif // JPEGENCODER_H
