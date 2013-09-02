#include "streamtime.h"

#include <QTime>

/**
	\class StreamTime

	\brief Bu sinif LMM icindeki zaman olcmeye ait yapilari
	sunar.

	Not: bu sinif QTime alternatifi degildir, kod bloklari
	uzerinde zaman olcumleri icin QTime kullanimi daha uygundur.

	StreamTime bir akisda gecen sureyi tutar. Ses ve goruntu
	senkronizasyonun saglanmasinda onemli bir rol tutar. Buna
	ek olarak akis baslangicindan itibaren gecen toplam sure, su
	anki akis zamani gibi farkli sure ayarlarina erisebilirsiniz.

	Bu siniftan butun bir oynatici da 1 adet vardir, ve oynatici(ya
	da kodlayi, kaydedici) altinda calisan butun elemanlara bu sinifi
	bildirir. StreamTime yaratilmasi elemanlarin kontrolunde degildir,
	olmamalidir.

	setCurrentTime() fonksiyonu akis zamanini degistir, bu fonksiyon genelde
	de-mux yapan elemanlar tarafindan kullanilir. getCurrentTime() fonksiyonu
	o anki akis zamanini micro-saniye cinsinden dondurur. getCurrentTimeMili()
	fonksiyonu ayni sureyi mili-saniye olarak dondurur. getFreeRunningTime()
	fonksiyonu ise akisin basladigi zamandan itibaren gecen sureyi
	dondurur. getCurrentTime() monotonik olmayabilir, ama getFreeRunningTime()
	akis devam ettigi surece monotoniktir.

	\ingroup lmm
*/

StreamTime::StreamTime(QObject *parent) :
	QObject(parent)
{
	startPts = 0;
	startTime = 0;
	drifter = new QTime;
	clock = new QTime;
	paused = false;
}

void StreamTime::setCurrentTime(qint64 val)
{
	currentTime = val;
	drifter->restart();
}

qint64 StreamTime::getCurrentTime()
{
	return getCurrentTimeMili() * 1000;
}

qint64 StreamTime::getCurrentTimeMili()
{
	if (paused)
		return currentTimePause;
	return currentTime / 1000 + drifter->elapsed();
}

void StreamTime::start()
{
	wallStartTime = QDateTime::currentDateTime();
	currentTime = 0;
	clock->restart();
	drifter->restart();
	setStartTime(0);
	setStartPts(0);
}

void StreamTime::stop()
{
}

void StreamTime::pause()
{
	currentTimePause = getCurrentTimeMili();
	paused = true;
}

void StreamTime::resume()
{
	if (!paused)
		return;
	setCurrentTime(currentTimePause * 1000);
	paused = false;
}

qint64 StreamTime::getFreeRunningTime()
{
	return clock->elapsed() * 1000ll;
}

qint64 StreamTime::ptsToStreamTime(qint64 pts)
{
	if (!startTime) {
		qint64 now = getCurrentTime();
		startPts = pts;
		startTime = now;
	}
	return startTime  + (pts - startPts);
}

qint64 StreamTime::ptsToTimeDiff(qint64 pts)
{
	qint64 now = getCurrentTime();
	qint64 time = ptsToStreamTime(pts);
	if (time < now)
		return 0;
	return time - now;
}

int StreamTime::getElapsedWallTime()
{
	return  wallStartTime.secsTo(QDateTime::currentDateTime());
}
