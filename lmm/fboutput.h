#ifndef FBOUTPUT_H
#define FBOUTPUT_H

#include "baselmmoutput.h"

#include <QList>
#include <QVariant>

class RawBuffer;
class QTime;
class BaseLmmElement;
struct Resize_Object;
struct _Buffer_Object;
typedef struct Resize_Object *Resize_Handle;
typedef struct _Buffer_Object *Buffer_Handle;

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
