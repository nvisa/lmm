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
	explicit DmaiEncoder(QObject *parent = 0);
	void setImageSize(QSize s);
	int start();
	int stop();
	int encode(Buffer_Handle buffer);
signals:
	
public slots:
private:
	Engine_Handle hEngine;
	Venc1_Handle hCodec;
	BufTab_Handle hBufTab;
	int numOutputBufs;
	int imageWidth;
	int imageHeight;

	int startCodec();
	int stopCodec();
};

#endif // DMAIENCODER_H
