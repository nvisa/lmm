#include "unittimestat.h"

#include <QTime>
#include <QDebug>

UnitTimeStat::UnitTimeStat()
{
	t = new QTime;
	avgMethod = COUNT;
	reset();
}

UnitTimeStat::UnitTimeStat(UnitTimeStat::AvgMethod m)
{
	t = new QTime;
	avgMethod = m;
	reset();
}

void UnitTimeStat::addStat(int value)
{
	if (value > max)
		max = value;
	if (value < min)
		min = value;
	total += value;
	last = value;
	avgCount++;
	if (avgMethod == COUNT) {
		if (avgCount == avgMax) {
			avg = total / avgCount;
			total = 0;
			avgCount = 0;
		}
	} else {
		int elapsed = t->elapsed();
		if (elapsed > 1000) {
			avgTime = total * 1000 / elapsed;
			t->restart();
			total = 0;
		}
	}
}

void UnitTimeStat::reset()
{
	total = 0;
	avg = 0;
	min = 0xffff;
	max = 0;
	last = 0;
	avgCount = 0;
	avgMax = 100;
	t->restart();
}
