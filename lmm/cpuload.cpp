#include "cpuload.h"

#include <emdesk/debug.h>

#include <QTimer>

#include <ti/sdo/dmai/Cpu.h>

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
	handle = Cpu_create(NULL);
	total = 0;
	count = 0;
	avg = 0;
	load = 0;
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
	timer->start(PERIOD);
	timer->setSingleShot(false);
}

int CpuLoad::getCpuLoad()
{
	if (!instance)
		instance = new CpuLoad;
	return instance->load;
}

void CpuLoad::timeout()
{
	Cpu_getLoad(handle, &load);
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
	return instance->avg;
}
