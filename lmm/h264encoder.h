#ifndef H264ENCODER_H
#define H264ENCODER_H

#include "dmaiencoder.h"

struct IH264VENC_DynamicParams;

class H264Encoder : public DmaiEncoder
{
	Q_OBJECT
public:
	explicit H264Encoder(QObject *parent = 0);
	int flush();

	int getMaxFrameRate() { return maxFrameRate / 1000; }
	int setMaxFrameRate(int v) { maxFrameRate = v * 1000; dirty = true; return 0; }
	RateControl getBitrateControlMethod() { return rateControl; }
	int getBitrate() { return videoBitRate; }
	int setBitrateControl(RateControl v) { rateControl = v; dirty = true; return 0; }
	int setBitrate(int v) { videoBitRate = v; dirty = true; return 0; }
	int getIntraFrameInterval() { return intraFrameInterval; }
	int setIntraFrameInterval(int v) { intraFrameInterval = v; dirty = true; return 0; }

	/* sei information */
	void setSeiLoopLatency(int lat);
signals:
	
public slots:
private:
	int seiLoopLatency;
	int seiBufferSize;
	IH264VENC_DynamicParams *dynH264Params;

	int encode(Buffer_Handle buffer, const RawBuffer source);
	int startCodec();
	int stopCodec();
	int addSeiData(QByteArray *ba, const RawBuffer source);
};

#endif // H264ENCODER_H
