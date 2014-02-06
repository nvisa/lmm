#ifndef UNITTIMESTAT_H
#define UNITTIMESTAT_H

class QTime;

class UnitTimeStat
{
public:
	enum AvgMethod {
		TIME,
		COUNT
	};
	explicit UnitTimeStat();
	explicit UnitTimeStat(AvgMethod m);
	void addStat(int value);
	void addStat();
	void startStat();
	void reset();
	int min;
	int max;
	int avg;

	/* internal variables */
	long long total;
	int last;
	int avgCount;
	int avgMax;
	int avgTime;
	QTime *t;
	QTime *addTime;
	AvgMethod avgMethod;
private:
};

#endif // UNITTIMESTAT_H
