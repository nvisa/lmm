#include "lmmthread.h"
#include "platform_info.h"
#include "debug.h"
#include "baselmmelement.h"

#include <QList>
#include <QMutex>

static QMutex mutex;
static QList<LmmThread *> threads;

LmmThread::LmmThread(QString threadName, BaseLmmElement *parent)
{
	this->parent = parent;
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
	instStack1 = (intptr_t *)&_trstack;
	instStack2 = (intptr_t *)&_trstack2;
	instPos = &_trpos;

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
	int opstat = 0;
	while (!exit) {
		st = IN_OPERATION;
		opstat = operation();
		if (opstat)
			break;
		lock.lock();
		time.restart();
		lock.unlock();
		if (paused) {
			st = PAUSED;
		}
	}
	mDebug("exiting thread %s(%p), status is %d", qPrintable(name)
		   , QThread::currentThreadId(), opstat);
	mutex.lock();
	threads.removeOne(this);
	mutex.unlock();
	st = QUIT;
	if (parent)
		parent->threadFinished(this);
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

void LmmThread::printStack()
{
	qDebug("%s has %d entries at %p:", qPrintable(name), *instPos, instPos);
	for (int i = 0; i < *instPos; i++) {
#if 1
		intptr_t *s1 = instStack1;
		intptr_t *s2 = instStack2;
		//if (s1[i] < 0 || s1[i] > 0x100000)
			//continue;
		qDebug("\t0x%x 0x%x", s1[i], s2[i]);
#else
		int *es = instStack1;
		int e = es[i];
		qDebug("\tfile=%d line=%d", e & 0xffff, e >> 16);
#endif
	}
}
