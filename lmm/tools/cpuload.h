#ifndef CPULOAD_H
#define CPULOAD_H

#include <QObject>

class QFile;
class QTimer;

class CpuLoad : public QObject
{
	Q_OBJECT
public:
	static int getAverageCpuLoad();
	static int getCpuLoad();
	static int setAverageT0();
signals:
	
private slots:
	void timeout();
private:
	QTimer *timer;
	int total;
	int count;
	int avg;
	int load;
	int userTime;
	int prevTotal;
	int t0Idle;
	int t0NonIdle;
	int lastNonIdle;
	int lastIdle;
	QFile *proc;

	CpuLoad();
	static CpuLoad *instance;
	int getAvgLoadFromProc();
	int getInstLoadFromProc();
	int getLoadFromProcDmai();
};

#endif // CPULOAD_H
