#ifndef CPULOAD_H
#define CPULOAD_H

#include <QObject>

class QTimer;

struct Cpu_Object;

class CpuLoad : public QObject
{
	Q_OBJECT
public:
	static int getAverageCpuLoad();
	static int getCpuLoad();
signals:
	
private slots:
	void timeout();
private:
	QTimer *timer;
	int total;
	int count;
	int avg;
	int load;
	struct Cpu_Object *handle;

	CpuLoad();
	static CpuLoad *instance;
};

#endif // CPULOAD_H
