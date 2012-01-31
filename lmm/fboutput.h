#ifndef FBOUTPUT_H
#define FBOUTPUT_H

#include "baselmmoutput.h"

#include <QList>
#include <QVariant>

#include <ti/sdo/dmai/Resize.h>

class RawBuffer;
class QTime;
class BaseLmmElement;

class FbOutput : public BaseLmmOutput
{
	Q_OBJECT
public:
	explicit FbOutput(QObject *parent = 0);
	int outputBuffer(RawBuffer *buf);
	int start();
	int stop();
	int flush();
signals:
	
public slots:
private:
	int fd;
	int fbSize;
	unsigned char *fbAddr;
	int fbLineLen;
	int fbHeight;
	Resize_Handle hResize;
	Buffer_Handle fbOutBuf;
	bool resizerConfigured;

	int openFb(QString filename);
};

#endif // FBOUTPUT_H
