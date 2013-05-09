#include "baselmmelement.h"
#include "debug.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "tools/unittimestat.h"

#include <errno.h>

#include <QTime>
#include <QSemaphore>

/**
	\class BaseLmmElement

	\brief Bu sinif LMM icindeki siniflarin turetilecegi baz sinifdir.
	Butun siniflarda olmasi gereken minimum arayuzleri sunar. Durum
	bilgisi(state machine), thread yonetimi, tampon(buffer) yonetimi,
	akis istatistikleri(fps v.s.), durdurma, baslatma gibi islemleri
	yonetir.

	BaseLmmElement sinifinin pek cok fonksiyonu virtual olarak
	isaretlenmistir, yani gerek gormeniz durumunda kendi siniflarinizda
	bu fonksiyonlari yeniden tanimlayabilirsiniz. Yeniden tanimlarken
	ilgili fonksiyonun dokumantasyonuna bakmanizda fayda vardir, zira
	pek cok fonksiyon icin baz sinifin fonksiyonu cagirmaniz gerekebilir.

	\section Kalitma BaseLmmElement Sinifindan Kalitma

	BaseLmmElement sinifindan yeni bir sinif kalitirken cok fazla yapilmasi
	gereken birsey yoktur, zira baz sinif komple bir elemanda olmasi gereken
	herseyi yapabilir. Sizin tek yapmaniz gereken yeni veri uretimini ya da
	var olan tamponlarin islenmesini saglamaktir. Bunun icin en kolay yol
	nextBuffer() fonksiyonunu 'override' etmektir. Bu fonksiyon icinde
	yeni bir RawBuffer yaratip bunu outputBuffers listesine ekledikten
	sonra baz sinifin nextBuffer() fonksiyonu ile fonksiyonunuzdan donmeniz
	yeterli olacaktir.

	Buna ek olarak start() ve stop() fonksiyonlarini da 'override'
	edebilirsiniz. Bu sayede akis baslarken ve dururken yapmaniz gereken islemleri
	gerceklestirebilirsiniz. Yine baz sinif implementasyonlarini kendi
	implementasyonunuz icinden cagirmaniz gereklidir.

	Eger elemaninizin kendi yarattigi tamponlar silinirken yapmasi gereken
	islemler varsa(ornegin donanim kaynaklarinin birakilmasi gibi)
	aboutDeleteBuffer() fonksiyonunu override edebilirsiniz. Baz implementasyon
	hic bir islem yapmamaktadir.

	\section Durum Durum Yonetimi

	Herhangi bir LMM elemani 3 durumdan birinde olabilir:
		INIT
		STARTED
		STOPPED

	INIT durumundaki elemanlar henuz her hangi bir akis icin ayarlanmamis
	elemanlardir. Genelde bu durum akis baslamadan once gorulur.Bir eleman
	start() fonksiyonu ile baslatildiktan sonra STARTED durumuna gecer,
	stop() ile durdurulursa STOPPED durumuna gecer.

	\section Tampon Tampon Giris Cikislari

	Her hangi bir eleman addBuffer() fonksiyonu ile yeni bir tampon alabilir.
	Bu fonksiyon 'virtual' degildir. Bu fonksyion gelen tampon gecerli
	bir tampon ise bunu inputBuffers listesine ekler. Ayrica gelen tampon
	istatistiklerini de uygun bir sekilde gunceller. Bu fonksiyonu yeniden
	yazmaniza gerek yoktur, eger gelen tamponlara erismek istiyorsaniz
	inputBuffers listesini kullaniniz.

	Her hangi bir eleman isledigi tamponlari nextBuffer() fonksiyonu ile
	baska elemanlara gonderir, bu fonksiyon 'virtual' olarak isaretlenmistir
	ve kendi elemanlarinizda degistirmeniz gereken 'minimum' fonksiyondur.
	Baz sinif elemani outputBuffers listesine bakar ve sirada bekleyen
	tampon varsa bunu dondurur. Akis istatiskilerini de gunceller, ozellikle
	FPS hesaplamasi burada yapilir. O yuzden kendi override edilmis
	fonksiyonlarinizda baz sinifin fonksiyonunu cagirmaniz kiritiktir.

	\section Istatistik Istatistik Yonetimi

	BaseLmmElement sinifinin en faydali ozelliklerinden birisi de ilgili
	elemana ait akis istatistiklerini sunabilmesidir. Su an icin hesaplanan
	istatistikler:

		* Giren tampon sayisi
			Bu ozellik ile elemanin calisma suresince aldigi toplam
			tampon sayisi gorulebilir.
		* Cikan tampons sayisi
			Bu ozellik ile elemanin calisma suresince gonderdigi toplam
			tampon sayisi gorulebilir. Giren ve cikis tampon sayilari
			birlikte kullanildigi zaman elemanin kullandigi tamponlar ile
			ne yaptigini gosterir. Ornegin, bir encoder elemani icin bu 2
			sayi bir birine esit olabilir, bir demux elemani genellikle
			birbirine esit degildir, ve TextOverlay elemani icin her zaman
			birbirine esit olmalidir.
		* Giriste(islenmeyi) bekleyen tampon sayisi
			Bu sayi elemana diger elemanlardan gelmis fakat henuz islenmemis
			eleman sayisini gosterir. Normal olarak bu sayinin 0-1 gibi birsey
			olmasi idealdir; eger bu sayi cok buyurse ve artarsa bir birikim
			oldugu anlamina gelir. Birikim elemanin ya cok yavas olduguna
			ya da bir hata durumu olduguna isaret eder.
		* Cikista(gonderilmeyi) bekleyen tampon sayisi
			Bu sayi elemanin isleyip baska elemanlara gonderilmek uzere
			bekleyen tampon sayisini gosterir. Buradaki bir birikim bu elemandan
			sonraki elemanlarin calismasinda bir sikinti oldugunun isaretidir.
		* Kare sayisi(fps)
			Belki de en faydali istatistik olarak kare sayisi(fps) bu elemanin
			saniye de kac kare isleyebildigini gosterir. Bu sayi video karesi
			uzerinde calismayan elemanlar icin cok anlamli olmayabilir, fakat
			o elemanlar icin zaten diger istatistikler gerekli performans bilgisini
			verecektir. Fakat yine de bu elemanlar icin de anlamli bir FPS bilgisine
			ihtiyac duyulursa bu elemanlar calculateFps() fonksiyonunu 'override'
			edebilir.

			Video karesi uzerinde calisan elemanlar icin ise bu sayi en az akis icin
			istenen sayi kadar olmalidir. Akis icinde kullanilan tum elemanlar icinde
			fps'i en dusuk olan eleman akis fps'ni de belirler.
		* Tampon cikis gecikmesi(latency)
			Bu sayi biraz kafa karistirici olabilir. Bu sayi elemana giren bir tamponun
			cikisa ulasana kadar maruz kaldigi gecikme suresi DEGILDIR. Bu sayi elemanlarin
			ozellikle de LCD, ses karti gibi donanim kaynaklarina cikis veren elemanlarin
			tamponu cikisa gonderdikten sonra, tamponun kullaniciya ulasmasi esnasindaki
			(sesin duyulmasi, ekranda goruntunun gorunmesi v.b.) gecikme suresidir.
			Bu gecikme genelde isletim sistemi suruculerindeki beklemelerden ya da
			tamponlardan kaynaklanir. Bu deger ses video senkronizasyonunu olumsuz
			etkiliyebilecegi icin onemlidir ve senkronizasyonun onemli oldugu calma elemanlari
			tarafindan kullanilir.
	\ingroup lmm
*/

BaseLmmElement::BaseLmmElement(QObject *parent) :
	QObject(parent)
{
	state = INIT;
	receivedBufferCount = sentBufferCount = 0;
	streamTime = NULL;
	fpsTiming = new QTime;
	elementFps = fpsBufferCount = 0;
	fpsTiming->start();
	outputTimeStat = new UnitTimeStat;
	enabled = true;

	bufsem  << new QSemaphore;
	inbufsem << new QSemaphore;
}

int BaseLmmElement::addBuffer(RawBuffer buffer)
{
	if (buffer.size() == 0)
		return -EINVAL;
	inputLock.lock();
	inputBuffers << buffer;
	inputLock.unlock();
	inbufsem[0]->release();
	receivedBufferCount++;
	return 0;
}

RawBuffer BaseLmmElement::nextBuffer()
{
	RawBuffer buf;
	outputLock.lock();
	if (outputBuffers.size() != 0)
		buf = outputBuffers.takeFirst();
	outputLock.unlock();
	if (buf.size()) {
		sentBufferCount++;
		calculateFps();
		updateOutputTimeStats();
	}

	return buf;
}

RawBuffer BaseLmmElement::nextBuffer(int ch)
{
	return nextBuffer();
}

RawBuffer BaseLmmElement::nextBufferBlocking(int ch)
{
	bufsem[ch]->acquire();
	return nextBuffer(ch);
}

int BaseLmmElement::start()
{
	receivedBufferCount = sentBufferCount = 0;
	elementFps = fpsBufferCount = 0;
	fpsTiming->start();
	state = STARTED;
	lastOutputTimeStat = 0;
	outputTimeStat->reset();
	return 0;
}

int BaseLmmElement::stop()
{
	state = STOPPED;
	flush();
	return 0;
}

void BaseLmmElement::printStats()
{
	qDebug() << this << receivedBufferCount << sentBufferCount;
}

int BaseLmmElement::getInputBufferCount()
{
	inputLock.lock();
	int size = inputBuffers.size();
	inputLock.unlock();
	return size;
}

int BaseLmmElement::getOutputBufferCount()
{
	outputLock.lock();
	int size = outputBuffers.size();
	outputLock.unlock();
	return size;
}

int BaseLmmElement::getAvailableDuration()
{
	int availDuration = 0;
	for (int i = 0; i < inputBuffers.size(); i++)
		availDuration += inputBuffers.at(i).getDuration();
	return availDuration;
}

void BaseLmmElement::setEnabled(bool val)
{
	enabled = val;
}

bool BaseLmmElement::isEnabled()
{
	return enabled;
}

void BaseLmmElement::calculateFps()
{
	fpsBufferCount++;
	if (fpsTiming->elapsed() > 1000) {
		int elapsed = fpsTiming->restart();
		elementFps = fpsBufferCount * 1000 / elapsed;
		fpsBufferCount = 0;
	}
}

BaseLmmElement::RunningState BaseLmmElement::getState()
{
	return state;
}

int BaseLmmElement::flush()
{
	inputLock.lock();
	inputBuffers.clear();
	inputLock.unlock();
	outputLock.lock();
	outputBuffers.clear();
	outputLock.unlock();
	inbufsem[0]->acquire(inbufsem[0]->available());
	bufsem[0]->acquire(bufsem[0]->available());
	return 0;
}

int BaseLmmElement::setParameter(QString param, QVariant value)
{
	parameters.insert(param, value);
	return 0;
}

QVariant BaseLmmElement::getParameter(QString param)
{
	if (parameters.contains(param))
		return parameters[param];
	return QVariant();
}

void BaseLmmElement::updateOutputTimeStats()
{
	if (lastOutputTimeStat) {
		int diff = streamTime->getFreeRunningTime() - lastOutputTimeStat;
		mInfo("time diff in %s: curr=%d ave=%d", metaObject()->className(), diff, outputTimeStat->avg);
		outputTimeStat->addStat(diff);
	}
	lastOutputTimeStat = streamTime->getFreeRunningTime();
}
