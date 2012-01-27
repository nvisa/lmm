#ifndef DM365CAPTURE_H
#define DM365CAPTURE_H

#include "baselmmelement.h"

#include <QSize>

#include <xdc/std.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Capture.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/BufferGfx.h>

class DM365Capture : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit DM365Capture(QObject *parent = 0);
	QSize captureSize();
	int putFrame(Buffer_Handle handle);
	Buffer_Handle getFrame();
signals:
	
public slots:
private:
	int openCamera();
	int closeCamera();

	Capture_Handle hCapture;
	BufTab_Handle hBufTab;
	int imageWidth;
	int imageHeight;
};

#endif // DM365CAPTURE_H
