#include "dmaibuffer.h"
#include "baselmmelement.h"

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

DmaiBuffer::DmaiBuffer(QString mimeType, Buffer_Handle handle, BaseLmmElement *parent)
{
	d = new DmaiBufferData();
	d->myParent = parent;
	d->rawData = NULL;
	d->refData = false;

	d->mimeType = mimeType;
	init(handle);
}

DmaiBuffer::DmaiBuffer(QString mimeType, const void *data, int size, BufferGfx_Attrs *gfxAttrs, BaseLmmElement *parent)
{
	d = new DmaiBufferData();
	d->myParent = parent;
	d->rawData = NULL;
	d->refData = false;
	d->mimeType = mimeType;
	init(size, gfxAttrs);
	memcpy(this->data(), data, size);
}

DmaiBuffer::DmaiBuffer(QString mimeType, int size, BufferGfx_Attrs *gfxAttrs, BaseLmmElement *parent)
{
	d = new DmaiBufferData();
	d->myParent = parent;
	d->rawData = NULL;
	d->refData = false;
	d->mimeType = mimeType;
	init(size, gfxAttrs);
}

DmaiBuffer::DmaiBuffer(const RawBuffer &other) :
	RawBuffer(other)
{
}

DmaiBuffer::~DmaiBuffer()
{
}

Buffer_Handle DmaiBuffer::getDmaiBuffer()
{
	DmaiBufferData *dd = (DmaiBufferData *)d.data();
	return dd->dmaibuf;
}

BufferGfx_Attrs * DmaiBuffer::createGraphicAttrs(int width, int height, int pixFormat)
{
	BufferGfx_Attrs *gfxAttrs = new BufferGfx_Attrs;
	*gfxAttrs = BufferGfx_Attrs_DEFAULT;
	gfxAttrs->dim.x = 0;
	gfxAttrs->dim.y = 0;
	gfxAttrs->dim.width = width;
	gfxAttrs->dim.height = height;
	if (pixFormat == V4L2_PIX_FMT_UYVY) {
		gfxAttrs->colorSpace = ColorSpace_UYVY;
		gfxAttrs->dim.lineLength = Dmai_roundUp(gfxAttrs->dim.width * 2, 32);
	} else if (pixFormat == V4L2_PIX_FMT_NV12) {
		gfxAttrs->colorSpace = ColorSpace_YUV420PSEMI;
		gfxAttrs->dim.lineLength = Dmai_roundUp(gfxAttrs->dim.width, 32);
	}
	return gfxAttrs;
}

int DmaiBuffer::getBufferSizeFor(BufferGfx_Attrs *attrs)
{
	if (attrs->colorSpace == ColorSpace_UYVY)
		return attrs->dim.width * attrs->dim.height * 2;
	if (attrs->colorSpace == ColorSpace_YUV420PSEMI)
		return attrs->dim.width * attrs->dim.height * 3 / 2;
	return 0;
}

void DmaiBuffer::init(int size, BufferGfx_Attrs *gfxAttrs)
{
	/* buffer allocation */
	/*BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
	gfxAttrs.dim.x = 0;
	gfxAttrs.dim.y = 0;
	gfxAttrs.dim.width = width;
	gfxAttrs.dim.height = height;
	if (pixFormat == V4L2_PIX_FMT_UYVY) {
		gfxAttrs.colorSpace = ColorSpace_UYVY;
		gfxAttrs.dim.lineLength = Dmai_roundUp(gfxAttrs.dim.width * 2, 32);
		bufSize = gfxAttrs.dim.width * gfxAttrs.dim.height * 2;
	} else if (pixFormat == V4L2_PIX_FMT_NV12) {
		gfxAttrs.colorSpace = ColorSpace_YUV420PSEMI;
		gfxAttrs.dim.lineLength = Dmai_roundUp(gfxAttrs.dim.width, 32);
		bufSize = gfxAttrs.dim.width * gfxAttrs.dim.height * 3 / 2;
	}

	int bufSize;
	if (bufSize != size)
		qDebug("buffer size doesn't match graphic attributes");*/
	DmaiBufferData *dd = (DmaiBufferData *)d.data();
	dd->dmaibuf = Buffer_create(size, BufferGfx_getBufferAttrs(gfxAttrs));
	dd->bufferOwner = true;
	setRefData(d->mimeType, Buffer_getUserPtr(dd->dmaibuf), size);
	addBufferParameter("width", (int)gfxAttrs->dim.width);
	addBufferParameter("height", (int)gfxAttrs->dim.height);
	addBufferParameter("dmaiBuffer", (int)dd->dmaibuf);
}

void DmaiBuffer::init(Buffer_Handle handle)
{
	DmaiBufferData *dd = (DmaiBufferData *)d.data();
	dd->bufferOwner = false;
	dd->dmaibuf = handle;
	Buffer_Attrs attrs; //= BufferGfx_getBufferAttrs(&gfxAttrs);
	Buffer_getAttrs(dd->dmaibuf, &attrs);
	BufferGfx_Attrs *gfxAttrs = (BufferGfx_Attrs *)&attrs;
	int size = Buffer_getNumBytesUsed(dd->dmaibuf);
	if (!size)
		size = Buffer_getSize(dd->dmaibuf);
	setRefData(d->mimeType, Buffer_getUserPtr(dd->dmaibuf), size);
	addBufferParameter("width", (int)gfxAttrs->dim.width);
	addBufferParameter("height", (int)gfxAttrs->dim.height);
	addBufferParameter("dmaiBuffer", (int)dd->dmaibuf);
}

DmaiBufferData::~DmaiBufferData()
{
	if (rawData && !refData)
		delete [] rawData;
	if (myParent)
		myParent->aboutDeleteBuffer(parameters);
	if (bufferOwner)
		Buffer_delete(dmaibuf);
}
