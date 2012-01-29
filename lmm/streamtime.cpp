#include "streamtime.h"

#include <QTime>

StreamTime::StreamTime(QObject *parent) :
	QObject(parent)
{
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
	return currentTime + drifter->elapsed() * 1000;
}

void StreamTime::start()
{
	clock->restart();
}

void StreamTime::stop()
{
}

qint64 StreamTime::getFreeRunningTime()
{
	return clock->elapsed() * 1000ll;
}
