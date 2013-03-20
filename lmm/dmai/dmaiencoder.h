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
	int start();
	int stop();
	int flush();
	int encodeNext();
	int encodeNextBlocking();
	void aboutDeleteBuffer(const QMap<QString, QVariant> &params);

	/* control API */
	int setCodecType(CodecType type);
	CodecType getCodecType() { return codec; }
	virtual void setFrameRate(float fps);
	virtual float getFrameRate();

	static void initCodecEngine();
signals:
	
public slots:
protected:
	Engine_Handle hEngine;
	BufTab_Handle hBufTab;
	BufTab_Handle outputBufTab;
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

	virtual int startCodec();
	virtual int stopCodec();
	virtual int encode(Buffer_Handle buffer, const RawBuffer source);

private:
	IVIDENC1_DynamicParams *dynParams;
};

#endif // DMAIENCODER_H