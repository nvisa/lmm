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

void LmmThread::pause()
{
	paused = true;
}

void LmmThread::resume()
{
	if (!paused)
		return;
	paused = false;
}

void LmmThread::run()
{
	id = QThread::currentThreadId();
#ifdef Q_WS_QWS
	mDebug("starting thread %s(%p)", qPrintable(name)
		   , QThread::currentThreadId());
#else
	mDebug("starting thread %s(%lld)", qPrintable(name)
		   , QThread::currentThreadId());
#endif
	time.start();
	exit = false;
	paused = false;
	while (!exit) {
		st = IN_OPERATION;
		if (operation())
			break;
		lock.lock();
		time.restart();
		lock.unlock();
		if (paused) {
			st = PAUSED;
		}
	}
	mDebug("exiting thread %s(%p)", qPrintable(name)
		   , QThread::currentThreadId());
	mutex.lock();
	threads.removeOne(this);
	mutex.unlock();
	st = QUIT;
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

LmmThread * LmmThread::getById(Qt::HANDLE id)
{
	foreach (LmmThread *th, threads) {
		if (th->id == id)
			return th;
	}
	return NULL;
}

LmmThread::Status LmmThread::getStatus()
{
	return st;
}

int LmmThread::elapsed()
{
	lock.lock();
	int elapsed = time.elapsed();
	lock.unlock();
	return elapsed;
}
