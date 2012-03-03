#ifndef DM365DMAICAPTURE_H
#define DM365DMAICAPTURE_H

#include "baselmmelement.h"

#include <QSize>

#include <xdc/std.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Capture.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/BufferGfx.h>

class captureThread;

class DM365DmaiCapture : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit DM365DmaiCapture(QObject *parent = 0);
	QSize captureSize();
	int putFrame(Buffer_Handle handle);
	Buffer_Handle getFrame();

	virtual int start();
	virtual int stop();
	virtual RawBuffer * nextBuffer();
	int finishedBuffer(RawBuffer *buf);
	void aboutDeleteBuffer(RawBuffer *buf);
signals:
	
public slots:
private:
	int openCamera();
	int closeCamera();

	captureThread *cThread;
	Capture_Handle hCapture;
	BufTab_Handle hBufTab;
	int imageWidth;
	int imageHeight;

	int captureCount;
};

#endif // DM365DMAICAPTURE_H
