#include "rawbuffer.h"
#include "baselmmelement.h"

#include <errno.h>

#include <QVariant>

/**
	\class RawBuffer

	\brief RawBuffer sinifi, tum LMM elementleri icinde kullanilan
	tampon(buffer) yapisidir. Cok esnek bir siniftir ve pek cok farkli
	bicimde kullanilabilir. Ozellikle donanim hafizalarinin
	etrafinda bir kaplayici olarak kullanilirsa sifir kopyalama
	ile gerekli islemlerin yapilmasini saglayabilir.

	RawBuffer sinifi 'Explicitly Shared' bir siniftir, yani kendi
	otomatik olarak referanslarini sayar ve hicbir kullanici kalmadigi
	zaman hafizadan silinir. Eger tampon bir donanim hafizasina bagli
	ise tampona sahip olan BaseLmmElement sinif ornegi aboutDeleteBuffer()
	fonksiyonu ile durumdan bilgilendirilir ve donanim kaynaklari
	serbest birakilabilir. Bu ozellikleri sayesinde RawBuffer orneklerini
	stack'te yaratabilir ve bir elemandan oburune cok hizli bir sekilde
	pointer kullanmadan gecebilirsiniz.

	RawBuffer sinifini bir ornek olustururken 2 metod kullanilabilir. Ya
	'size' parametresi alan uygun bir yapilandirici kullanirsiniz; bu
	durumda RawBuffer hafizadan gerekli yeri ayirir ve yok olurken
	serbest birakir. 2. methodda ise yapilandirici fonksiyona boyut
	bilgisi gecmez, daha sonra setRefData() fonksiyonu ile kullanacagi
	hafizayi gosterirsiniz. Bu durumda ornek yok olurken hafizayi serbest
	birakmaz.

	Buna ek olark tampon ile ilgili bilgileri de bu sinif icinde tutabilirsiniz.
	Nasil ki GStreamer icindeki GstBuffer elemanlarinin 'caps' parametreleri
	ile tamponda tutalan veri tipine ait bilgileri bulunur, RawBuffer icinde de
	bu meta bilgiler tutulur. Bunun icin elemanlar addBufferParameter() ve
	getBufferParameter() fonksiyonlarini kullanirlar. Su an icin su bilgiler
	tanimlanmistir:

		- mimeType (QString)
			- Zorunlu
			- IANA MIME isimlendirmesini mumkun oldugunca takip etmeli.
			  GStreamer dokumantasyonu referans olarak kullanilabilir.
			  http://gstreamer.freedesktop.org/data/doc/gstreamer/head/pwg/html/section-types-definitions.html
			- Farkli mime tipleri icin farkli parametreler tanimlanmistir. Mime tiplerine gore
			  parametreler asagida verilmistir:

			- video/x-raw-yuv
				* width (int)
				* height (int)
				* fps (float)
				* v4l2PixelFormat (int -> V4L2_PIX_FMT)
			- video/x-h264
				* frameType(int)
					I Frame -> 0
					P Frame -> 1
					B Frame -> 2
					IDR Frame -> 3
				* fps (float)
		- v4l2Buffer (int)
			Eger RawBuffer bir v4l2'den gelen bir tampon ise bunun ile ilgili
			v4l2Buffer structure'ina ait pointer degeri bu parametre ile tutulur.
		- captureTime (int)
			Eger soz konusu tampon v4l2 ile yakalanmis(capture) bir tampon ise,
			bu deger capture elemani tarafindan set edilir. Birim olarak streamTime
			tarafindan dondurulen degerdir.

	\ingroup lmm

	\sa DmaiBuffer
*/

RawBuffer::RawBuffer(QString mimeType, const void *data, int size, BaseLmmElement *parent)
{
	d = new RawBufferData;
	d->refData = false;
	d->rawData = NULL;
	d->mimeType = mimeType;
	setSize(size);
	memcpy(d->rawData + d->prependPos, data, size);
	d->myParent = parent;
}

RawBuffer::RawBuffer(QString mimeType, int size, BaseLmmElement *parent)
{
	d = new RawBufferData;
	d->refData = false;
	d->rawData = NULL;
	d->mimeType = mimeType;
	setSize(size);
	d->myParent = parent;
}

RawBuffer::RawBuffer(const RawBuffer &other)
	: d (other.d)
{
}

void RawBuffer::setParentElement(BaseLmmElement *el)
{
	d->myParent = el;
}

RawBuffer::RawBuffer(BaseLmmElement *parent)
{
	d = new RawBufferData;
	d->myParent = parent;
	d->rawData = NULL;
	d->refData = false;
}

RawBuffer::~RawBuffer()
{
}

void RawBuffer::setRefData(QString mimeType, void *data, int size)
{
	if (d->rawData && !d->refData)
		delete [] d->rawData;
	d->rawData = (char *)data;
	d->rawDataLen = size;
	d->refData = true;
	d->prependPos = 0;
	d->mimeType = mimeType;
	d->usedLen = size;
}

void RawBuffer::addBufferParameter(QString par, QVariant val)
{
	d->parameters.insert(par, val);
}

QVariant RawBuffer::getBufferParameter(QString par) const
{
	if (d->parameters.contains(par))
		return d->parameters[par];
	return QVariant();
}

void RawBuffer::setSize(int size)
{
	if (d->rawData && !d->refData)
		delete [] d->rawData;
	d->prependLen = d->prependPos = 4096;
	d->appendLen = 0;
	d->rawData = new char[d->prependLen + size + d->appendLen];
	d->rawDataLen = size;
	d->usedLen = size;
}

int RawBuffer::prepend(const void *data, int size)
{
	if (d->refData)
		return -EPERM;
	if (d->prependPos < size)
		return -EINVAL;
	memcpy(d->rawData + d->prependPos - size, data, size);
	d->prependPos -= size;
	d->rawDataLen += size;
	return 0;
}

const void *RawBuffer::constData() const
{
	return d->rawData + d->prependPos;
}

void *RawBuffer::data()
{
	return d->rawData + d->prependPos;
}

int RawBuffer::size() const
{
	return d->usedLen;
}

int RawBuffer::setUsedSize(int size)
{
	if (size > d->rawDataLen)
		return -EINVAL;
	d->usedLen = size;
	return 0;
}

void RawBuffer::setDuration(unsigned int val)
{
	d->duration = val;
}

unsigned int RawBuffer::getDuration() const
{
	return d->duration;
}

void RawBuffer::setPts(qint64 val)
{
	d->pts = val;
}

qint64 RawBuffer::getPts() const
{
	return d->pts;
}

void RawBuffer::setDts(qint64 val)
{
	d->dts = val;
}

qint64 RawBuffer::getDts() const
{
	return d->dts;
}

QString RawBuffer::getMimeType() const
{
	return d->mimeType;
}

void RawBuffer::setStreamBufferNo(int val)
{
	d->bufferNo = val;
}

int RawBuffer::streamBufferNo() const
{
	return d->bufferNo;
}

RawBufferData::~RawBufferData()
{
	if (rawData && !refData)
		delete [] rawData;
	if (myParent)
		myParent->aboutDeleteBuffer(parameters);
}
