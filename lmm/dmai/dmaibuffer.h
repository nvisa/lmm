#ifndef DMAIBUFFER_H
#define DMAIBUFFER_H

#include "rawbuffer.h"

struct _Buffer_Object;
typedef struct _Buffer_Object *Buffer_Handle;

class DmaiBuffer : public RawBuffer
{
public:
	explicit DmaiBuffer(void *data, int size, BaseLmmElement *parent = 0);
	explicit DmaiBuffer(int size, BaseLmmElement *parent = 0);
	~DmaiBuffer();
signals:
	
public slots:
private:
	int pixFormat;
	Buffer_Handle dmaibuf;

	void init(int size);
};

#endif // DMAIBUFFER_H
