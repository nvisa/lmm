#include "baselmmelement.h"
#include "debug.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "tools/unittimestat.h"

#include <errno.h>

#include <QElapsedTimer>
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

class RateReducto {
public:
	RateReducto(int skip, int outOf)
	{
		enabled = true;
		this->skip = skip;
		total = outOf;
		reset();
	}
	void reset()
	{
		current = 0;
	}
	bool shouldSkip()
	{
		if (!enabled)
			return false;
		if (current++ < skip)
			return true;
		if (current >= total - 1)
			reset();
		return false;
	}

	int skip;
	int total;
	bool enabled;
	int current;
};

static void createQueues(QList<ElementIOQueue *> *list, int cnt, BaseLmmElement *el)
{
	while (list->size() < cnt) {
		ElementIOQueue *q = el->createIOQueue();
		q->start();
		*list << q;
	}
}

BaseLmmElement::BaseLmmElement(QObject *parent) :
	QObject(parent)
{
	state = INIT;
	streamTime = NULL;
	processTimeStat = new UnitTimeStat(UnitTimeStat::COUNT);
	enabled = true;
	passThru = false;
	inBufSize << 0;
	evHook = NULL;
}

int BaseLmmElement::addBuffer(int ch, const RawBuffer &buffer)
{
	inql.lock();
	createQueues(&inq, ch + 1, this);
	ElementIOQueue *queue = inq[ch];
	inql.unlock();
	return queue->addBuffer(buffer, this);
}

RawBuffer BaseLmmElement::nextBufferBlocking(int ch)
{
	QMutexLocker l(&outql);
	return outq[ch]->getBuffer(this);
}

int BaseLmmElement::process(int ch)
{
	inql.lock();
	createQueues(&inq, ch + 1, this);
	ElementIOQueue *queue = inq[ch];
	inql.unlock();
	RawBuffer buf = queue->getBufferNW(this);
	if (buf.size()) {
		processTimeStat->startStat();
		notifyEvent(EV_PROCESS, buf);
		int ret = processBuffer(ch, buf);
		processTimeStat->addStat();
		return ret;
	}
	return 0;
}

int BaseLmmElement::process(int ch, const RawBuffer &buf)
{
	inql.lock();
	createQueues(&inq, ch + 1, this);
	ElementIOQueue *queue = inq[ch];
	inql.unlock();
	int err = queue->addBuffer(buf, this);
	if (err)
		return err;
	return process(ch);
}

int BaseLmmElement::processBlocking(int ch)
{
	inql.lock();
	createQueues(&inq, ch + 1, this);
	ElementIOQueue *queue = inq[ch];
	inql.unlock();
	RawBuffer buf = queue->getBuffer(this);
	if (!buf.size())
		return -ENOENT;

tryagain:
	int ret = 0;
	processTimeStat->startStat();
	if (!ch)
		ret = processBuffer(buf);
	else
		ret = processBuffer(ch, buf);
	processTimeStat->addStat();

	if (ret == -EAGAIN)
		goto tryagain;

	notifyEvent(EV_PROCESS, buf);

	return ret;
}

StreamTime *BaseLmmElement::getStreamTime()
{
	return streamTime;
}

int BaseLmmElement::start()
{
	eofSent = false;
	state = STARTED;

	for (int i = 0; i < inq.size(); i++)
		inq[i]->start();
	for (int i = 0; i < outq.size(); i++)
		outq[i]->start();
	return 0;
}

int BaseLmmElement::stop()
{
	return prepareStop();
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

QList<QVariant> BaseLmmElement::extraDebugInfo()
{
	return QList<QVariant>();
}

void BaseLmmElement::addOutputQueue(ElementIOQueue *q)
{
	QMutexLocker l(&outql);
	outq << q;
}

ElementIOQueue *BaseLmmElement::getOutputQueue(int ch)
{
	QMutexLocker l(&outql);
	createQueues(&outq, ch + 1, this);
	return outq[ch];
}

void BaseLmmElement::addInputQueue(ElementIOQueue *q)
{
	QMutexLocker l(&inql);
	inq << q;
}

ElementIOQueue *BaseLmmElement::getInputQueue(int ch)
{
	QMutexLocker l(&inql);
	createQueues(&inq, ch + 1, this);
	return inq[ch];
}

ElementIOQueue *BaseLmmElement::createIOQueue()
{
	return new ElementIOQueue;
}

int BaseLmmElement::getInputQueueCount()
{
	QMutexLocker l(&inql);
	return inq.size();
}

int BaseLmmElement::getOutputQueueCount()
{
	QMutexLocker l(&outql);
	return outq.size();
}

int BaseLmmElement::setSetting(const QString &setting, const QVariant &value)
{
	Q_UNUSED(value);
	Q_UNUSED(setting);
	return 0;
}

QVariant BaseLmmElement::getSetting(const QString &setting)
{
	Q_UNUSED(setting);
	return QVariant();
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

int BaseLmmElement::newOutputBuffer(int ch, const RawBuffer &buf)
{
	QMutexLocker l(&outql);
	createQueues(&outq, ch + 1, this);
	return outq[ch]->addBuffer(buf, this);
}

int BaseLmmElement::newOutputBuffer(int ch, QList<RawBuffer> list)
{
	QMutexLocker l(&outql);
	createQueues(&outq, ch + 1, this);
	return outq[ch]->addBuffer(list, this);
}

void BaseLmmElement::notifyEvent(BaseLmmElement::Events ev, const RawBuffer &buf)
{
	evLock.lock();
	if (evHook)
		(*evHook)(this, buf, ev, evPriv);
	evLock.unlock();
}

int BaseLmmElement::flush()
{
	for (int i = 0; i < inq.size(); i++)
		inq[i]->clear();
	for (int i = 0; i < outq.size(); i++)
		outq[i]->clear();
	return 0;
}

int BaseLmmElement::prepareStop()
{
	if (state == STOPPED) {
		mDebug("element %s already stopped", metaObject()->className());
		return 0;
	}
	state = STOPPED;
	for (int i = 0; i < inq.size(); i++)
		inq[i]->stop();
	for (int i = 0; i < outq.size(); i++)
		outq[i]->stop();
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

void BaseLmmElement::threadFinished(LmmThread *th)
{
	BaseLmmElement *parentEl = qobject_cast<BaseLmmElement *>(parent());
	if (parentEl)
		parentEl->threadFinished(th);
}

void BaseLmmElement::setEventHook(BaseLmmElement::eventHook hook, void *priv)
{
	evHook = hook;
	evPriv = priv;
}

int BaseLmmElement::processBuffer(int ch, const RawBuffer &buf)
{
	Q_UNUSED(ch);
	Q_UNUSED(buf);
	return 0;
}

ElementIOQueue::ElementIOQueue()
{
	evHook = NULL;
	state = BaseLmmElement::INIT;

	bufSem = new QSemaphore;
	outWc = new QWaitCondition();
	bufSize = 0;
	bufSizeLimit = 0;
	receivedCount = sentCount = 0;

	fpsTiming = new QElapsedTimer;
	fpsTiming->start();
	fps = 0;
	fpsBufferCount = 0;

	rc = NULL;
}

int ElementIOQueue::waitBuffers(int lessThan)
{
	if (state == BaseLmmElement::STOPPED)
		return -EINVAL;
	lock.lock();
	int size = queue.size();
	if (size >= lessThan) {
		outputWakeThreshold = lessThan;
		outWc->wait(&lock);
	}
	lock.unlock();
	if (state == BaseLmmElement::STOPPED)
		return -EINVAL;
	return 0;
}

int ElementIOQueue::addBuffer(const RawBuffer &buffer, BaseLmmElement *src)
{
	if (buffer.size() == 0)
		return -EINVAL;
	lock.lock();
	int err = checkSizeLimits();
	if (err) {
		lock.unlock();
		return err;
	}

	bool skip = false;
	if (rc)
		skip = rc->shouldSkip();

	if (!skip) {
		queue << buffer;
		bufSize += buffer.size();
	}
	receivedCount++;
	calculateFps();
	lock.unlock();
	notifyEvent(EV_ADD, buffer, src);
	if (!skip)
		bufSem->release();
	return 0;
}

int ElementIOQueue::addBuffer(const QList<RawBuffer> &list, BaseLmmElement *src)
{
	for (int i = 0; i < list.size(); i++) {
		int err = addBuffer(list[i], src);
		if (err)
			return err;
	}
	return 0;
}

int ElementIOQueue::prependBuffer(const RawBuffer &buffer)
{
	lock.lock();
	queue.prepend(buffer);
	bufSize += buffer.size();
	bufSem->release();
	lock.unlock();
	return 0;
}

RawBuffer ElementIOQueue::getBuffer(BaseLmmElement *src)
{
	if (!acquireSem())
		return RawBuffer();
	return getBufferNW(src);
}

RawBuffer ElementIOQueue::getBufferNW(BaseLmmElement *src)
{
	RawBuffer buf;
	lock.lock();
	if (queue.size() != 0)
		buf = queue.takeFirst();
	bufSize -= buf.size();
	lock.unlock();
	if (buf.size()) {
		sentCount++;
		notifyEvent(EV_GET, buf, src);
	}

	return buf;
}

int ElementIOQueue::getSemCount() const
{
	return bufSem->available();
}

int ElementIOQueue::getBufferCount()
{
	lock.lock();
	int size = queue.size();
	lock.unlock();
	return size;
}

void ElementIOQueue::clear()
{
	lock.lock();
	bufSem->acquire(bufSem->available());
	queue.clear();
	bufSize = 0;
	lock.unlock();
}

void ElementIOQueue::start()
{
	outputWakeThreshold = 0;
	receivedCount = sentCount = 0;
	fps = 0;
	fpsBufferCount = 0;
	fpsTiming->start();
	state = BaseLmmElement::STARTED;
}

void ElementIOQueue::stop()
{
	state = BaseLmmElement::STOPPED;
	clear();
	bufSem->release(50);
}

int ElementIOQueue::setSizeLimit(int size, int hsize)
{
	lock.lock();
	bufSizeLimit = size;
	hysterisisSize = hsize;
	lock.unlock();
	return 0;
}

void ElementIOQueue::setEventHook(ElementIOQueue::eventHook hook, void *priv)
{
	evLock.lock();
	evPriv = priv;
	evHook = hook;
	evLock.unlock();
}

int ElementIOQueue::setRateReduction(int skip, int outOf)
{
	if (!rc)
		delete rc;
	rc = new RateReducto(skip, outOf);
	return 0;
}

bool ElementIOQueue::acquireSem()
{
	if (state == BaseLmmElement::STOPPED)
		return false;
	bufSem->acquire();
	if (state == BaseLmmElement::STOPPED)
		return false;
	return true;
}

int ElementIOQueue::checkSizeLimits()
{
	if (!bufSizeLimit)
		return 0; //no size checking
	if (bufSize >= bufSizeLimit)
		return -ENOSPC;
	return 0;
}

void ElementIOQueue::calculateFps()
{
	fpsBufferCount++;
	if (fpsTiming->elapsed() > 1000) {
		int elapsed = fpsTiming->restart();
		fps = fpsBufferCount * 1000 / elapsed;
		fpsBufferCount = 0;
	}
}

void ElementIOQueue::notifyEvent(Events ev, const RawBuffer &buf, BaseLmmElement *src)
{
	evLock.lock();
	if (evHook)
		(*evHook)(this, buf, ev, src, evPriv);
	evLock.unlock();
}
