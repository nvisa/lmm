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
	int start();
	int stop();
	int flush();
	int encodeNext();
	void aboutDeleteBuffer(const QMap<QString, QVariant> &params);

	/* control API */
	int setCodecType(CodecType type);
	CodecType getCodecType() { return codec; }
	void setImageSize(QSize s);
	QSize getImageSize() { return QSize(imageWidth, imageHeight); }

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

	virtual int startCodec() = 0;
	virtual int stopCodec() = 0;
	virtual int encode(Buffer_Handle buffer) = 0;
};

#endif // DMAIENCODER_H
