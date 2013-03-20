#include "circularbuffer.h"
#define DEBUG
#include "debug.h"

#include <errno.h>

#include <QMutex>
#include <QThread>
#include <QSemaphore>

/**
	\class CircularBuffer

	\brief CircularBuffer sinifi isminden de anlasilabilecegi gibi basit
	circular buffer gerceklemesidir.

	Ilk olarak uygun bir yaratici fonksiyon kullanarak bir tampon ornegi
	yaratmalisiniz. Var olan bir hafiza kullanilabilir, ya da tampon tarafindan
	yeni bir hafiza yaratilabilir.

	Daha sonra addData() fonksiyonu ile tampona yeni veri ekleyebilirsiniz, okuma
	yapmak icin getDataPointer() fonksiyonunu kullanabilirsiniz. getDataPointer()
	fonksiyonu size bir adres dondurecegi icin bu fonksiyonu memcpy gibi islemlerde
	rahatlikla kullanabilirsiniz. getDataPointer() hafiza yapisini degistirmedigi
	icin tekrar tekrar kullanilabilir.

	Tampondan veri bosaltmak icin useData() fonksiyonunu kullanabilirsiniz. reset()
	fonksiyonu ile her hangi bir anda tamponu ilk baslangic durumuna cevirebilirsiniz.

	\section Thread Coklu is parcaciklari (Multithreading) destegi

	Bu tampon icindeki fonksiyonlar coklu is parcaciklarindan cagirmak icin uygun degildir,
	hiz icin optimize edilmislerdir. Eger uygulamaniz coklu is parcacigini destekliyorsa
	(ki cogunlukla oyledir) tampon fonksiyonlarini cagirmadan once lock() fonksiyonu cagirip
	isiniz bitince unlock() kullanabilirsiniz. Boylece tamponu coklu is parcacigi kullanilan
	ortamlarda rahatlikla kullanabilirsiniz.

	\ingroup lmm
*/

CircularBuffer::CircularBuffer(QObject *parent) :
	QObject(parent)
{
	mutex = new QMutex;
	waitSem = new QSemaphore;
}

/**
 * @brief CircularBuffer::CircularBuffer Var olan bir hafizayi kullanan ornek yaratir.
 * @param source Var olan hafizanin adresi, ornegin yasam dongusu boyunca silinmemeli.
 * @param size Varolan hafizanin boyutu.
 * @param parent Sinifa sahip olan ust sinif, NULL olabilir.
 *
 * Bu yaratici fonksiyon ile olusturulan ornek verilen adresteki hafizayi kullanir.
 * Sinif yok edilirken hafiza yok EDILMEZ.
 */
CircularBuffer::CircularBuffer(void *source, int size, QObject *parent) :
	QObject(parent)
{
	dataOwner = false;
	rawData = (char *)source;
	rawDataLen = size;
	mutex = new QMutex;
	lockThread = 0;
	waitSem = new QSemaphore;
	reset();
}

/**
 * @brief Verilen boyutta bir ornek yaratir.
 * @param size Istenen tampon boyutu.
 * @param parent Sinifa sahip olan ust sinif, NULL olabilir.
 *
 * Bu yaritici fonksiyon ile olusturulan ornek yok edilirken butun sahip oldugu hafizalari da siler.
 */
CircularBuffer::CircularBuffer(int size, QObject *parent) :
	QObject(parent)
{
	dataOwner = true;
	rawData = new char[size];
	rawDataLen = size;
	mutex = new QMutex;
	lockThread = 0;
	waitSem = new QSemaphore;
	reset();
}

CircularBuffer::~CircularBuffer()
{
	if (dataOwner && rawData) {
		delete rawData;
		rawData = NULL;
	}
	if (mutex) {
		delete mutex;
		mutex = NULL;
	}
}

/**
 * @brief Tampondan okumak yapmak icin gerekli adresi dondurur.
 * @return Bir sonraki okunabilir hafizayi dondurur.
 *
 * Tampon icindeki veriyi degistirmek icin bu fonksiyonu kullanmayiniz. Bu fonksiyon sadece
 * okumada kullanilmak uzere tasarlanmistir. Bu fonksiyon cagrildiktan sonra
 * tampondaki hafiza yapisi degismez. Eger okunan verinin kullanilmasini
 * istiyorsaniz bu fonksiyondan sonra useData() fonksiyonunu cagirabilirsiniz.
 *
 * \sa addData, useData
 */
void * CircularBuffer::getDataPointer()
{
	return tail;
}

/**
 * @brief Tampon icinde ne kadar veri oldugunu ogrenmek icin kullanabilirsiniz.
 * @return Kullanilan hafiza miktarini dondurur.
 *
 * Bu fonksiyon tampon icinde ne kadar veri oldugunu gosterir, ya da tamponun icindeki
 * kullanilmis veri miktarina isaret eder. Tampon dolu iken 0 dondurur, bosken totalSize()
 * kadar deger dondurur.
 *
 * \sa freeSize(), totalSize()
 */
int CircularBuffer::usedSize()
{
	return usedBufLen;
}

/**
 * @brief Tampon icindeki kullanilabilir alani gosterir.
 * @return Bos hafiza miktarini dondurur.
 *
 * Bu fonksiyon tampon icindeki kullanilabilir alani dondurur.
 *
 * \sa usedSize(), totalSize()
 */
int CircularBuffer::freeSize()
{
	return freeBufLen;
}

/**
 * @brief Butun tampon boyutunu gosterir.
 * @return Tamponun toplam boyutunu dondurur.
 *
 * Bu fonksiyon tamponun yaratildigi andaki 'size' degeri kadardir. Her zaman
 * usedSize() + freeSize() toplamina esittir.
 *
 * \sa usedSize(), freeSize()
 */
int CircularBuffer::totalSize()
{
	return rawDataLen;
}

/**
 * @brief Tampondan verilen boyut kadar veriyi kullanir.
 * @param size Kullanilmak istenen veri miktari.
 * @return Gercek olarak kullanilan veri miktarini dondurur.
 *
 * Bu fonksiyon tampondan 'size' kadar veriyi kullanir ve tamponda o kadar
 * yer acilir. Bu cagridan sonra usedSize() verilen kadar daha az deger
 * dondurur, freeSize() daha cok.
 *
 * Eger kullanilmak istenen veri miktari tampondakinden fazla ise
 * tampondaki veri kadar kullanilir.
 *
 * Bu fonksiyon cok hizlidir.
 *
 * \sa addData(), usedSize(), freeSize()
 */
int CircularBuffer::useData(int size)
{
	if (size > usedBufLen)
		size = usedBufLen;
	tail += size;
	if (tail > rawData + rawDataLen) {
		tail -= rawDataLen;
		mDebug("tail passed end of circ buf, resetting");
	}
	freeBufLen += size;
	usedBufLen -= size;

	return size;
}

/**
 * @brief Tampona yeni veri eklemek icin kullanilir.
 * @param data Tampona eklenecek verinin adresi.
 * @param size Tampona eklenecek verinin boyutu.
 * @return Hata yoksa '0', hata durumunda negatif hata kodu.
 *
 * Bu fonksiyonu tampona veri eklemek icin kullanabilirsiniz. Veri tampon
 * icindeki hafizaya kopyalanir. Eger tampondan yeteri kadar yer yoksa
 * EINVAL hata kodu dondurur ve hicbir islem yapmaz.
 *
 * Bu fonksiyon tampon icinde kaydirmaya yol acabilir.
 *
 * \sa useData()
 */
int CircularBuffer::addData(const void *data, int size)
{
	/* do not overwrite */
	if (size > freeBufLen)
		return -EINVAL;

	if (head + size > rawData + rawDataLen) {
		if (usedBufLen) {
			mDebug("no space left, shifting %d bytes out of %d bytes", usedBufLen, rawDataLen);
			/* At this point it is guarenteed that head > tail due to not overriding */
			/* TODO: Following memcpy may be optimized */
			memcpy(rawData, tail, usedBufLen);
			head = rawData + usedBufLen;
			tail = rawData;
		} else
			reset();
	}
	memcpy(head, data, size);
	head += size;
	freeBufLen -= size;
	usedBufLen += size;
	waitSem->release(size);

	return 0;
}

/**
 * @brief Tamponu ilk kullanim haline dondurur.
 * @return Hata yoksa '0', hata durumunda negatif hata kodu.
 *
 * Tampondaki tum veriyi bosaltir, tamamen bos olarak yeniden
 * kullanilabilir duruma getirir. Bu fonksiyon cok hizlidir.
 *
 * \sa useData()
 */
int CircularBuffer::reset()
{
	freeBufLen = rawDataLen;
	head = tail = rawData;
	usedBufLen = 0;
	waitSem->acquire(waitSem->available());

	return 0;
}

/**
 * @brief Tamponda yeteri kadar veri toplanmasini bekler.
 * @param Beklenecek veri miktari.
 * @return Hata yoksa '0', hata durumunda negatif hata kodu.
 *
 * Bu fonksiyon tamponda verilen veri miktari birikene kadar
 * bekler. Coklu is parcaciklari icinden beklemek icin bu
 * fonksiyon kullanilabilir.
 *
 * Her hangi bir sekilde tamponun hafiza yapisini degistirmez.
 * Bu fonksiyon reset() ile birlikte kullanilabilir.
 *
 * \sa useData()
 */
int CircularBuffer::wait(int size)
{
	waitSem->acquire(size);
	return 0;
}

/**
 * @brief Tampon icindeki mutex'i kitler.
 */
void CircularBuffer::lock()
{
	if (!lockThread)
		lockThread = QThread::currentThreadId();
	else if (lockThread == QThread::currentThreadId())
		mDebug("locking from thread 0x%lx twice, deadlock huh :)", QThread::currentThreadId());
	mutex->lock();
}

/**
 * @brief Tampon icindeki mutex'i serbest birakir.
 */
void CircularBuffer::unlock()
{
	lockThread = 0;
	mutex->unlock();
}

