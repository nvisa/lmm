#include "streamtime.h"

#include <QTime>

StreamTime::StreamTime(QObject *parent) :
	QObject(parent)
{
	startPts = 0;
	startTime = 0;
	drifter = new QTime;
	clock = new QTime;
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
	return currentTime + drifter->elapsed();
}

void StreamTime::start()
{
	currentTime = 0;
	clock->restart();
	drifter->restart();
	setStartTime(0);
	setStartPts(0);
}

void StreamTime::stop()
{
}

qint64 StreamTime::getFreeRunningTime()
{
	return clock->elapsed() * 1000ll;
}
