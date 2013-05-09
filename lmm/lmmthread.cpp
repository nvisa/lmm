#include "lmmthread.h"
#include "platform_info.h"
#include "debug.h"

#include <QList>
#include <QMutex>

static QMutex mutex;
static QList<LmmThread *> threads;

LmmThread::LmmThread(QString threadName)
{
	name = threadName;
	mutex.lock();
	threads << this;
	mutex.unlock();
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
	mDebug("exiting thread %s(%p)", qPrintable(name)
		   , QThread::currentThreadId());
	mutex.lock();
	threads.removeOne(this);
	mutex.unlock();
}

void LmmThread::stopAll()
{
	foreach (LmmThread *th, threads) {
		qDebug("stopping thread %s", qPrintable(th->name));
		th->stop();
	}
	foreach (LmmThread *th, threads) {
		qDebug("waiting thread %s", qPrintable(th->name));
		th->wait(1000);
	}
}
