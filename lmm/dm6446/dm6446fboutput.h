#ifndef BLEC32FBOUTPUT_H
#define BLEC32FBOUTPUT_H

#include <lmm/fboutput.h>

struct Resize_Object;
struct _Buffer_Object;
typedef struct Resize_Object *Resize_Handle;
typedef struct _Buffer_Object *Buffer_Handle;

class DM6446FbOutput : public FbOutput
{
	Q_OBJECT
public:
	explicit DM6446FbOutput(QObject *parent = 0);
	int outputBuffer(RawBuffer buf);
	int start();
	int stop();
	int flush();
signals:
	
public slots:
private:
	Resize_Handle hResize;
	Buffer_Handle fbOutBuf;
	bool resizerConfigured;
};

#endif // BLEC32FBOUTPUT_H
