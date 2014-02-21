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
	aboutToDeleteBuffer() fonksiyonunu override edebilirsiniz. Baz implementasyon
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

	\section Threading ve Hiz Kontrolu

	BaseLmmElement icindeki varsayilan davranis bicimi non-blocking operasyondur.
	Buna gore addBuffer() ve nextBuffer() fonksiyonlari uyumazlar. Eger buffer
	eklemek icin yeterince yer yoksa(ya da hiz limiti asilmissa) addBuffer()
	hic bir islem yapmaz, ayni sekilde yeni tampon yoksa nextBuffer() fonksiyonu
	bos bir RawBuffer dondurur. Bu durum bazi durumlarda istenen davranis olmayabilir.
	Ornegin bu multi-threaded bir uygulamada kullanilirken arzu edilen davranis bicimi
	bu fonksiyonlarin bir buffer gelene kadar ya da yer acilana kadar uyumasidir.
	Bunun icin blocking API'yi kullanabilirsiniz. addBufferBlocking() ve
	nextBufferBlocking() fonksiyonlari karsilik geldikleri fonksiyonlarin uyuyan
	versiyonlaridir. Eger yeterli yer yoksa addBufferBlocking() fonksiyonu yer acilana
	kadar uyur, ayni sekilde dondurecek bir tampon yoksa nextBufferBlocking() fonksiyonu
	uyumaya gecer. Uyuma islemleri semaphore kullanilarak yapilir, hem cok az CPU kullanilir
	hem de Unix sinyalleri ile uyumlu calisirlar.

	Varsayilan olarak BaseLmmElement sinifi hiz kontrolu yapmaz. Gelen butun tamponlar
	kuyruga eklenir ve islenir. Bu bazi durumlarda kullanilan hafizanin buyumesine ve
	sistem hafizasi yetersiz hale gelir ise sistemin calismasinin aksamasina neden olabilir.
	Bunun onune gecmek icin setTotalInputBufferSize() fonksiyonu ile alinacak bufferlar hakkinda
	bir ust sinir koyabilirsiniz. Eger belirli bir sinirlama konursa addBuffer() fonksiyonu
	hata mesaji donrururken, addBufferBlocking() fonksiyonu yer acilana kadar uyumaya
	gecer.

	\section Elemanlarin Durdurulup Baslatilmasi

	LMM elememanlari threaded uygulamalarda kullanilabilmektedir. Bunun icin blocking API
	destegi eklenmistir. Bu sayede threadler icinde elemanlar uyuyup yeni veri hazir olunca
	uyanmaktadirlar. Bu yapinin sorun cikarabilecegi noktalardan birisi oynaticilarin durdurulup
	baslatilmasi islemidir. Burada ki sorunlarin onune gecmek icin turetilen elemanlarin uymasi
	gereken bazi kurallar sunlardir:

		* Kalittiginiz siniflarda acquireInputSem() ve acquireOutputSem() fonksiyonlarini kullanarak
		  uyumaya geciniz; bu fonksiyonlarin donus degerlerini mutlaka kontrol ediniz.
		* flush() fonksiyonunu overload ederken en son da mutlaka baz implementasyonu cagirarak
		  donus yapin.

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
	processTimeStat = new UnitTimeStat(UnitTimeStat::COUNT);
	enabled = true;
	totalInputBufferSize = 0;

	bufsem  << new QSemaphore;
	inbufsem << new QSemaphore;
	inBufQueue << QList<RawBuffer>();
	outBufQueue << QList<RawBuffer>();
	outWc << new QWaitCondition();
	inBufSize << 0;
}

int BaseLmmElement::addBuffer(int ch, RawBuffer buffer)
{
	if (buffer.size() == 0)
		return -EINVAL;
	inputLock.lock();
	int err = checkSizeLimits();
	if (err) {
		inputLock.unlock();
		return err;
	}
	inBufQueue[ch] << buffer;
	inBufSize[ch] += buffer.size();
	receivedBufferCount++;
	inputLock.unlock();
	inbufsem[ch]->release();
	if (buffer.isEOF())
		return -ENODATA;
	return 0;
}

int BaseLmmElement::addBufferBlocking(int ch, RawBuffer buffer)
{
	if (!buffer.size())
		return -EINVAL;
	inputLock.lock();
	while (checkSizeLimits()) {
		/* no space left on device, wait */
		inputWaiter.wait(&inputLock);
	}
	inBufQueue[ch] << buffer;
	inBufSize[ch] += buffer.size();
	inputLock.unlock();
	inbufsem[ch]->release();
	receivedBufferCount++;
	return 0;
}

int BaseLmmElement::addBuffersBlocking(int ch, const QList<RawBuffer> list)
{
	inputLock.lock();
	inBufQueue[ch] << list;
	foreach (const RawBuffer buf, list)
		inBufSize[ch] += buf.size();
	inbufsem[ch]->release(list.size());
	inputLock.unlock();
	return 0;
}

RawBuffer BaseLmmElement::nextBuffer(int ch)
{
	RawBuffer buf;
	outputLock.lock();
	if (outBufQueue[ch].size() != 0)
		buf = outBufQueue[ch].takeFirst();
	outputLock.unlock();
	if (buf.size()) {
		sentBufferCount++;
		calculateFps(buf);
		updateOutputTimeStats();
		checkAndWakeInputWaiters();
		if (outputWakeThreshold && bufsem[ch]->available() < outputWakeThreshold)
			outWc[ch]->wakeAll();
	}

	return buf;
}

RawBuffer BaseLmmElement::nextBufferBlocking(int ch)
{
	if (!acquireOutputSem(ch))
		return RawBuffer();
	return nextBuffer(ch);
}

QList<RawBuffer> BaseLmmElement::nextBuffers(int ch)
{
	QList<RawBuffer> list;
	outputLock.lock();
	list = outBufQueue[ch];
	outBufQueue[ch].clear();
	outputLock.unlock();
	return list;
}

QList<RawBuffer> BaseLmmElement::nextBuffersBlocking(int ch)
{
	QList<RawBuffer> list;
	if (!acquireOutputSem(ch))
		return QList<RawBuffer>();
	outputLock.lock();
	list = outBufQueue[ch];
	outBufQueue[ch].clear();
	bufsem[ch]->acquire(bufsem[ch]->available());
	sentBufferCount += list.size();
	outputLock.unlock();
	foreach (RawBuffer buf, list)
		calculateFps(buf);
	updateOutputTimeStats();
	checkAndWakeInputWaiters();
	if (outputWakeThreshold && bufsem[ch]->available() < outputWakeThreshold)
		outWc[ch]->wakeAll();
	return list;
}

int BaseLmmElement::process(int ch)
{
	if (inbufsem[ch]->tryAcquire(1)) {
		RawBuffer buf = takeInputBuffer(ch);
		if (buf.size()) {
			processTimeStat->startStat();
			int ret = processBuffer(buf);
			processTimeStat->addStat();
			return ret;
		}
	}
	return 0;
}

int BaseLmmElement::process(int ch, RawBuffer buf)
{
	int err = addBuffer(ch, buf);
	if (err)
		return err;
	return process(ch);
}

int BaseLmmElement::processBlocking(int ch)
{
	if (!acquireInputSem(ch))
		return -EINVAL;
	RawBuffer buf = takeInputBuffer(ch);
	if (buf.isEOF()) {
		mDebug("new eof received, forwarding and quitting");
		newOutputBuffer(ch, buf);
		return -ENODATA;
	}
	if (!buf.size())
		return -ENOENT;
	int ret = 0;
	processTimeStat->startStat();
	if (!ch)
		ret = processBuffer(buf);
	else
		ret = processBuffer(ch, buf);
	processTimeStat->addStat();
	return ret;
}

int BaseLmmElement::processBlocking(int ch, RawBuffer buf)
{
	int err = addBuffer(ch, buf);
	if (err)
		return err;
	return processBlocking(ch);
}

int BaseLmmElement::sendEOF()
{
	eofSent = true;
	for (int i = 0; i < outBufQueue.size(); i++)
		newOutputBuffer(i, RawBuffer::eof());
	return 0;
}

int BaseLmmElement::start()
{
	eofSent = false;
	outputWakeThreshold = 0;
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
	if (state == STOPPED) {
		mDebug("element %s already stopped", metaObject()->className());
		return 0;
	}
	state = STOPPED;
	flush();
	for (int i = 0; i < inbufsem.size(); i++)
		inbufsem[i]->release(50);
	for (int i = 0; i < bufsem.size(); i++) {
		bufsem[i]->release(50);
		outWc[i]->wakeAll();
	}
	return 0;
}

void BaseLmmElement::printStats()
{
	qDebug() << this << receivedBufferCount << sentBufferCount;
}

int BaseLmmElement::getInputBufferCount()
{
	inputLock.lock();
	int size = inBufQueue[0].size();
	inputLock.unlock();
	return size;
}

int BaseLmmElement::getOutputBufferCount()
{
	outputLock.lock();
	int size = outBufQueue[0].size();
	outputLock.unlock();
	return size;
}

int BaseLmmElement::getAvailableDuration()
{
	int availDuration = 0;
	for (int i = 0; i < inBufQueue[0].size(); i++)
		availDuration += inBufQueue[0].at(i).constPars()->duration;
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

bool BaseLmmElement::isRunning()
{
	if (getState() == STARTED)
		return true;
	return false;
}

int BaseLmmElement::getInputSemCount(int ch)
{
	return inbufsem[ch]->available();
}

int BaseLmmElement::getOutputSemCount(int ch)
{
	return bufsem[ch]->available();
}

QList<QVariant> BaseLmmElement::extraDebugInfo()
{
	return QList<QVariant>();
}

void BaseLmmElement::calculateFps(const RawBuffer buf)
{
	Q_UNUSED(buf);
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

int BaseLmmElement::setState(BaseLmmElement::RunningState s)
{
	state = s;
	return 0;
}

/**
 * @brief BaseLmmElement::checkSizeLimits
 * @return Returns -ENOSPC if no space left, 0 otherwise.
 *
 * This function doesn't lock mutexes so make sure that mutexes are locked
 * while calling this function.
 */
int BaseLmmElement::checkSizeLimits()
{
	if (!totalInputBufferSize)
		return 0; //no size checking
	int size = inBufSize[0];
	mInfo("size=%d total=%d", size, totalInputBufferSize);
	if (size >= totalInputBufferSize)
		return -ENOSPC;
	return 0;
}

void BaseLmmElement::checkAndWakeInputWaiters()
{
	if (!totalInputBufferSize)
		return;

	inputLock.lock();
	int size = 0;
	foreach (RawBuffer buf, inBufQueue[0])
		size += buf.size();
	inputLock.unlock();
	if (size < totalInputBufferSize - inputHysterisisSize)
		inputWaiter.wakeAll();
}

int BaseLmmElement::newOutputBuffer(int ch, RawBuffer buf)
{
	outputLock.lock();
	outBufQueue[ch] << buf;
	releaseOutputSem(ch);
	outputLock.unlock();
	return 0;
}

int BaseLmmElement::newOutputBuffer(int ch, QList<RawBuffer> list)
{
	outputLock.lock();
	outBufQueue[ch] << list;
	releaseOutputSem(ch, list.size());
	outputLock.unlock();
	return 0;
}

int BaseLmmElement::waitOutputBuffers(int ch, int lessThan)
{
	if (getState() == STOPPED)
		return -EINVAL;
	outputLock.lock();
	int size = outBufQueue[0].size();
	if (size >= lessThan) {
		outputWakeThreshold = lessThan;
		outWc[ch]->wait(&outputLock);
	}
	outputLock.unlock();
	if (getState() == STOPPED)
		return -EINVAL;
	return 0;
}

int BaseLmmElement::flush()
{
	inputLock.lock();
	for (int i = 0; i < inbufsem.size(); i++) {
		inbufsem[i]->acquire(inbufsem[i]->available());
		inBufQueue[i].clear();
		inBufSize[i] = 0;
	}
	inputLock.unlock();
	outputLock.lock();
	for (int i = 0; i < bufsem.size(); i++) {
		bufsem[i]->acquire(bufsem[i]->available());
		outBufQueue[i].clear();
	}
	outputLock.unlock();
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

int BaseLmmElement::setTotalInputBufferSize(int size, int hysterisisSize)
{
	inputLock.lock();
	totalInputBufferSize = size;
	inputHysterisisSize = hysterisisSize;
	inputLock.unlock();
	return 0;
}

void BaseLmmElement::updateOutputTimeStats()
{
	if (!streamTime)
		return;
	if (lastOutputTimeStat) {
		int diff = streamTime->getFreeRunningTime() - lastOutputTimeStat;
		mLog("time diff in %s: curr=%d ave=%d", metaObject()->className(), diff, outputTimeStat->avg);
		outputTimeStat->addStat(diff);
	}
	lastOutputTimeStat = streamTime->getFreeRunningTime();
}


bool BaseLmmElement::acquireInputSem(int ch)
{
	if (state == STOPPED)
		return false;
	inbufsem[ch]->acquire();
	if (state == STOPPED)
		return false;
	return true;
}

bool BaseLmmElement::acquireOutputSem(int ch)
{
	if (state == STOPPED)
		return false;
	bufsem[ch]->acquire();
	if (state == STOPPED)
		return false;
	return true;
}

RawBuffer BaseLmmElement::takeInputBuffer(int ch)
{
	inputLock.lock();
	RawBuffer buf;
	if (inBufQueue[ch].size()) {
		buf = inBufQueue[ch].takeFirst();
		inBufSize[ch] -= buf.size();
	}
	inputLock.unlock();
	return buf;
}

int BaseLmmElement::appendInputBuffer(int ch, RawBuffer buf)
{
	inputLock.lock();
	inBufQueue[ch].append(buf);
	inBufSize[ch] += buf.size();
	inbufsem[ch]->release();
	inputLock.unlock();
	return 0;
}

int BaseLmmElement::prependInputBuffer(int ch, RawBuffer buf)
{
	inputLock.lock();
	inBufQueue[ch].prepend(buf);
	inBufSize[ch] += buf.size();
	inbufsem[ch]->release();
	inputLock.unlock();
	return 0;
}

int BaseLmmElement::processBuffer(int ch, RawBuffer buf)
{
	Q_UNUSED(ch);
	Q_UNUSED(buf);
	return 0;
}

int BaseLmmElement::releaseInputSem(int ch, int count)
{
	inbufsem[ch]->release(count);
	return 0;
}

int BaseLmmElement::releaseOutputSem(int ch, int count)
{
	bufsem[ch]->release(count);
	return 0;
}

void BaseLmmElement::addNewInputChannel()
{
	inbufsem << new QSemaphore;
	inBufQueue << QList<RawBuffer>();
	inBufSize << 0;
}

void BaseLmmElement::addNewOutputChannel()
{
	bufsem << new QSemaphore;
	outBufQueue << QList<RawBuffer>();
	outWc << new QWaitCondition();
}
