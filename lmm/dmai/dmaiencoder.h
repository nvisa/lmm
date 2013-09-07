#ifndef DMAIENCODER_H
#define DMAIENCODER_H

#include <lmm/baselmmelement.h>

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

class UnitTimeStat;
class QTime;

#define CODECHEIGHTALIGN 16

class DmaiEncoder : public BaseLmmElement
{
	Q_OBJECT
public:
	enum CodecType {
		CODEC_MPEG2,
		CODEC_MPEG4,
		CODEC_H264,
		CODEC_JPEG
	};
	enum RateControl {
		RATE_CBR,
		RATE_VBR,
		RATE_NONE
	};
	explicit DmaiEncoder(QObject *parent = 0);
	virtual ~DmaiEncoder();
	int start();
	int stop();
	int flush();
	int genIdr();
	void aboutDeleteBuffer(const QMap<QString, QVariant> &params);

	/* control API */
	int setCodecType(CodecType type);
	CodecType getCodecType() { return codec; }
	virtual void setFrameRate(float fps);
	virtual float getFrameRate();
	int getMaxFrameRate() { return maxFrameRate / 1000; }
	int setMaxFrameRate(int v) { maxFrameRate = v * 1000; dirty = true; return 0; }
	RateControl getBitrateControlMethod() { return rateControl; }
	int getBitrate() { return videoBitRate; }
	int setBitrateControl(RateControl v) { rateControl = v; dirty = true; return 0; }
	int setBitrate(int v) { videoBitRate = v; dirty = true; return 0; }
	int getIntraFrameInterval() { return intraFrameInterval; }
	int setIntraFrameInterval(int v) { intraFrameInterval = v; dirty = true; return 0; }

	static void initCodecEngine();
	static void cleanUpDsp();
signals:
	
public slots:
protected:
	QMutex dspl;
	Engine_Handle hEngine;
	BufTab_Handle hBufTab;
	int numOutputBufs;
	int imageWidth;
	int imageHeight;
	int encodeCount;
	QMutex bufferLock;
	CodecType codec;
	UnitTimeStat *encodeTimeStat;
	QTime *encodeTiming;

	float frameRate;
	Venc1_Handle hCodec;
	int maxFrameRate;
	RateControl rateControl;
	int videoBitRate;
	int intraFrameInterval;
	bool generateIdrFrame;
	bool dirty;
	int inputPixFormat;

	virtual int startCodec();
	virtual int stopCodec();
	virtual int encode(Buffer_Handle buffer, const RawBuffer source);
	int processBuffer(RawBuffer buf);

private:
	IVIDENC1_DynamicParams *dynParams;
};

#endif // DMAIENCODER_H
