#include "cpuload.h"

#include "debug.h"

#include <QFile>
#include <QTimer>
/*
 * Note that load calculation operation can last some time
 * ~5 msec, so it is best to keep timer period maximum
 */
#define PERIOD 1000
#define COUNT_END (1000 / PERIOD)

CpuLoad *CpuLoad::instance = NULL;

CpuLoad::CpuLoad() :
	QObject()
{
	total = 0;
	count = 0;
	avg = 0;
	load = 0;
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
	timer->start(PERIOD);
	timer->setSingleShot(false);
	proc = NULL;
	userTime = 0;
	prevTotal = 0;
	t0Idle = t0NonIdle;
	lastIdle = lastNonIdle = 0;

	proc = new QFile("/proc/stat");
	proc->open(QIODevice::ReadOnly | QIODevice::Unbuffered);
}

int CpuLoad::getAvgLoadFromProc()
{
	proc->seek(0);
	QList<QByteArray> list = proc->readLine().split(' ');
	int nonIdle = list[2].toInt()
			+ list[3].toInt()
			+ list[4].toInt()
			+ list[6].toInt()
			+ list[7].toInt()
			+ list[8].toInt()
			+ list[9].toInt()
			;
	int idle = list[5].toInt();
	if (!t0NonIdle) {
		t0NonIdle = nonIdle;
		t0Idle = idle;
		return 0;
	}
	nonIdle -= t0NonIdle;
	idle -= t0Idle;
	return 100 * nonIdle / (nonIdle + idle);
}

int CpuLoad::getInstLoadFromProc()
{
	proc->seek(0);
	QList<QByteArray> list = proc->readLine().split(' ');
	int nonIdle = list[2].toInt()
			+ list[3].toInt()
			+ list[4].toInt()
			+ list[6].toInt()
			+ list[7].toInt()
			+ list[8].toInt()
			+ list[9].toInt()
			;
	int idle = list[5].toInt();
	int load = 100 * (nonIdle - lastNonIdle) / (nonIdle + idle - lastNonIdle - lastIdle);
	lastIdle = idle;
	lastNonIdle = nonIdle;
	return load;
}

int CpuLoad::getLoadFromProcDmai()
{
	proc->seek(0);
	QList<QByteArray> list = proc->readLine().split(' ');
	int utime = list[2].toInt()
			+ list[3].toInt()
			+ list[4].toInt()
			+ list[6].toInt()
			+ list[7].toInt()
			+ list[8].toInt()
			+ list[9].toInt()
			;
	int ttime = userTime + list[3].toInt()
			+ list[4].toInt()
			+ list[5].toInt()
			+ list[6].toInt()
			+ list[7].toInt()
			+ list[8].toInt()
			+ list[9].toInt()
			;
	int delta = ttime - prevTotal;
	prevTotal = ttime;
	int load = 0;
	if (delta)
		load = 100 * (utime - userTime) / delta;
	userTime = utime;

	return load;
}

int CpuLoad::getCpuLoad()
{
	if (!instance)
		instance = new CpuLoad;
	return instance->load;
}

int CpuLoad::setAverageT0()
{
	if (!instance)
		instance = new CpuLoad;
	instance->t0NonIdle = instance->t0Idle = 0;
	return instance->getAvgLoadFromProc();
}

void CpuLoad::timeout()
{
	load = getInstLoadFromProc();
	mInfo("cpu load is %%%d", load);
	total += load;
	count++;
	if (count >= COUNT_END) {
		avg = total / count;
		total = count = 0;
	}
}

int CpuLoad::getAverageCpuLoad()
{
	if (!instance)
		instance = new CpuLoad;
	return instance->getAvgLoadFromProc();
}
