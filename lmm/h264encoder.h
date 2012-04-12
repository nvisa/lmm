#ifndef H264ENCODER_H
#define H264ENCODER_H

#include "dmaiencoder.h"

class H264Encoder : public DmaiEncoder
{
	Q_OBJECT
public:
	enum RateControl {
		RATE_CBR,
		RATE_VBR,
		RATE_NONE
	};
	explicit H264Encoder(QObject *parent = 0);
	int flush();

	int getMaxFrameRate() { return maxFrameRate / 1000; }
	int setMaxFrameRate(int v) { maxFrameRate = v * 1000; return 0; }
	RateControl getBitrateControlMethod() { return rateControl; }
	int getBitrate() { return videoBitRate; }
	int setBitrateControl(RateControl v) { rateControl = v; return 0; }
	int setBitrate(int v) { videoBitRate = v; return 0; }
	int getIntraFrameInterval() { return intraFrameInterval; }
	int setIntraFrameInterval(int v) { intraFrameInterval = v; return 0; }
signals:
	
public slots:
private:
	Venc1_Handle hCodec;
	bool generateIdrFrame;
	int maxFrameRate;
	RateControl rateControl;
	int videoBitRate;
	int intraFrameInterval;
	VIDENC1_DynamicParams   defaultDynParams;

	int encode(Buffer_Handle buffer);
	int startCodec();
	int stopCodec();
};

#endif // H264ENCODER_H
