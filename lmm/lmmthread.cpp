#include "lmmthread.h"

#include <emdesk/platform_info.h>

LmmThread::LmmThread(QString threadName)
{
	name = threadName;
}

void LmmThread::stop()
{
	exit = true;
}

void LmmThread::run()
{
#ifdef Q_WS_QWS
	qDebug("starting thread %s(%p)", qPrintable(name)
		   , QThread::currentThreadId());
#else
	qDebug("starting thread %s(%lld)", qPrintable(name)
		   , QThread::currentThreadId());
#endif
	exit = false;
	while (!exit) {
		if (operation())
			break;
	}
}
