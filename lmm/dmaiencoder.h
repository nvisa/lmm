#ifndef DMAIENCODER_H
#define DMAIENCODER_H

#include "baselmmelement.h"

#include <QSize>

#include <xdc/std.h>

#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/Pause.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/ce/Venc1.h>

class DmaiEncoder : public BaseLmmElement
{
	Q_OBJECT
public:
	enum CodecType {
		CODEC_MPEG2,
		CODEC_MPEG4,
		CODEC_H264,
	};
	enum RateControl {
		RATE_CBR,
		RATE_VBR,
		RATE_NONE
	};
	explicit DmaiEncoder(QObject *parent = 0);
	int start();
	int stop();
	int flush();
	int encodeNext();
	int encode(Buffer_Handle buffer);
	void aboutDeleteBuffer(const QMap<QString, QVariant> &params);

	/* control API */
	int setCodecType(CodecType type);
	CodecType getCodecType() { return codec; }
	void setImageSize(QSize s);
	QSize getImageSize() { return QSize(imageWidth, imageHeight); }
	int getMaxFrameRate() { return maxFrameRate / 1000; }
	int setMaxFrameRate(int v) { maxFrameRate = v * 1000; return 0; }
	RateControl getBitrateControlMethod() { return rateControl; }
	int getBitrate() { return videoBitRate; }
	int setBitrateControl(RateControl v) { rateControl = v; return 0; }
	int setBitrate(int v) { videoBitRate = v; return 0; }
	int getIntraFrameInterval() { return intraFrameInterval; }
	int setIntraFrameInterval(int v) { intraFrameInterval = v; return 0; }

	static void initCodecEngine();
signals:
	
public slots:
private:
	Engine_Handle hEngine;
	Venc1_Handle hCodec;
	BufTab_Handle hBufTab;
	BufTab_Handle outputBufTab;
	int numOutputBufs;
	int imageWidth;
	int imageHeight;
	int encodeCount;
	bool generateIdrFrame;
	int maxFrameRate;
	QMutex bufferLock;
	CodecType codec;
	RateControl rateControl;
	int videoBitRate;
	int intraFrameInterval;

	VIDENC1_DynamicParams   defaultDynParams;

	int startCodec();
	int stopCodec();
};

#endif // DMAIENCODER_H
