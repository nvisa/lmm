#include "lmmthread.h"

#include <emdesk/platform_info.h>
#include <emdesk/debug.h>

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
	mDebug("starting thread %s(%p)", qPrintable(name)
		   , QThread::currentThreadId());
#else
	mDebug("starting thread %s(%lld)", qPrintable(name)
		   , QThread::currentThreadId());
#endif
	exit = false;
	while (!exit) {
		if (operation())
			break;
	}
}
