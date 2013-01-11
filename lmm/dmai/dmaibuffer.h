#ifndef DMAIBUFFER_H
#define DMAIBUFFER_H

#include "rawbuffer.h"

struct _Buffer_Object;
typedef struct _Buffer_Object *Buffer_Handle;
struct BufferGfx_Attrs;

class DmaiBufferData : public RawBufferData
{
public:
	DmaiBufferData()
		: RawBufferData()
	{
		bufferOwner = true;
	}
	DmaiBufferData(const DmaiBufferData &other)
		: RawBufferData(other)
	{
		bufferOwner = other.bufferOwner;
		dmaibuf = other.dmaibuf;
	}
	~DmaiBufferData();
	Buffer_Handle dmaibuf;
	bool bufferOwner;
};

class DmaiBuffer : public RawBuffer
{
public:
	explicit DmaiBuffer(QString mimeType, Buffer_Handle handle, BaseLmmElement *parent = 0);
	explicit DmaiBuffer(QString mimeType, const void *data, int size, BufferGfx_Attrs *gfxAttrs, BaseLmmElement *parent = 0);
	explicit DmaiBuffer(QString mimeType, int size, BufferGfx_Attrs *gfxAttrs, BaseLmmElement *parent = 0);
	DmaiBuffer(const RawBuffer &other);
	~DmaiBuffer();

	Buffer_Handle getDmaiBuffer();

	static BufferGfx_Attrs * createGraphicAttrs(int width, int height, int pixFormat);
	static int getBufferSizeFor(BufferGfx_Attrs *attrs);
signals:
	
public slots:
protected:
private:
	int pixFormat;

	void init(int size, BufferGfx_Attrs *gfxAttrs);
	void init(Buffer_Handle handle);
};

#endif // DMAIBUFFER_H
