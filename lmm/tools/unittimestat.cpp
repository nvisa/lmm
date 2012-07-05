#include "unittimestat.h"

UnitTimeStat::UnitTimeStat()
{
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
	if (avgCount == avgMax) {
		avg = total / avgCount;
		total = 0;
		avgCount = 0;
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
}
