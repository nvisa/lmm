#ifndef UNITTIMESTAT_H
#define UNITTIMESTAT_H

class UnitTimeStat
{
public:
	explicit UnitTimeStat();
	void addStat(int value);
	void reset();
	int min;
	int max;
	int avg;

	/* internal variables */
	int total;
	int last;
	int avgCount;
	int avgMax;
private:
};

#endif // UNITTIMESTAT_H
