#include "dmaibuffer.h"

#include <emdesk/debug.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>

#include <linux/videodev2.h>

/**
	\class DmaiBuffer

	\brief DmaiBuffer sinifi, RawBuffer sinifindan turetilmis ama DMAI
	yapilarini icinde tasiyan bir siniftir. Tipki RawBuffer gibi
	referans sayar. Bu sinif TI islemcilerine ozgu bir siniftir.

	DmaiBuffer sinifi RawBuffer sinifi uzerinde ince bir katmandir.
	Veriyi DMAI yapilari kullanarak yaratir, yani Buffer_create
	kullanarak. RawBuffer da her zaman referans alacak sekilde yaratilir.
	Sinif silinecegi zaman otomatik olarak Buffer_delete ile dmai
	donanim memorisini de serbest birakir.

	Bu sinif gerekli tampon parametrelerini de ekler. Bu sinifi
	kullaniyorsaniz getBufferParameter() fonksiyonu ile su parametrelere
	erisebilirsiniz:

		* width -> Videonun genisligi (int)
		* height -> Videonun yuksekligi (int)
		* dmaiBuffer -> Alt tarafta kullanilan dmai tamponu (int) (Buffer_Handle)

	\ingroup lmm

	\sa RawBuffer
*/

DmaiBuffer::DmaiBuffer(void *data, int size, BaseLmmElement *parent) :
	RawBuffer(parent)
{
	pixFormat = V4L2_PIX_FMT_NV12;
	init(size);
	memcpy(this->data(), data, size);
}

DmaiBuffer::DmaiBuffer(int size, BaseLmmElement *parent) :
	RawBuffer(parent)
{
	pixFormat = V4L2_PIX_FMT_NV12;
	init(size);
}

DmaiBuffer::DmaiBuffer(const RawBuffer &other) :
	RawBuffer(other)
{
}

DmaiBuffer::~DmaiBuffer()
{
}

void DmaiBuffer::init(int size)
{
	/* buffer allocation */
	BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
	gfxAttrs.dim.x = 0;
	gfxAttrs.dim.y = 0;
	gfxAttrs.dim.width = 1280; //TODO:
	gfxAttrs.dim.height = 720; //TODO:
	int bufSize;
	if (pixFormat == V4L2_PIX_FMT_UYVY) {
		gfxAttrs.colorSpace = ColorSpace_UYVY;
		gfxAttrs.dim.lineLength = Dmai_roundUp(gfxAttrs.dim.width * 2, 32);
		bufSize = gfxAttrs.dim.width * gfxAttrs.dim.height * 2;
	} else if (pixFormat == V4L2_PIX_FMT_NV12) {
		gfxAttrs.colorSpace = ColorSpace_YUV420PSEMI;
		gfxAttrs.dim.lineLength = Dmai_roundUp(gfxAttrs.dim.width, 32);
		bufSize = gfxAttrs.dim.width * gfxAttrs.dim.height * 3 / 2;
	}

	if (bufSize != size)
		qDebug("buffer size doesn't match graphic attributes");
	dmaibuf = Buffer_create(size, BufferGfx_getBufferAttrs(&gfxAttrs));

	setRefData(Buffer_getUserPtr(dmaibuf), bufSize);
	addBufferParameter("width", (int)gfxAttrs.dim.width);
	addBufferParameter("height", (int)gfxAttrs.dim.height);
	addBufferParameter("dmaiBuffer", (int)dmaibuf);
	addBufferParameter("dmaiBufferFree", (int)dmaibuf);
}
