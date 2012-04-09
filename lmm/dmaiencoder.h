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
	explicit DmaiEncoder(QObject *parent = 0);
	void setImageSize(QSize s);
	int setCodecType(CodecType type);
	int start();
	int stop();
	int flush();
	int encodeNext();
	int encode(Buffer_Handle buffer);
	void aboutDeleteBuffer(const QMap<QString, QVariant> &params);

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
	QMutex bufferLock;
	CodecType codec;

	VIDENC1_DynamicParams   defaultDynParams;

	int startCodec();
	int stopCodec();
};

#endif // DMAIENCODER_H
