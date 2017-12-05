#include "jobdistributorelement.h"

#include <lmm/debug.h>
#include <lmm/lmmthread.h>

#include <QSemaphore>

#include <unistd.h>

class DistHandlerThread : public QThread
{
public:
	void run()
	{
		busy = false;
		active = true;
		while (active) {
			wsem.acquire();
			if (!active)
				break;
			doJob();
			markBusy(false);
		}
	}

	bool isBusy()
	{
		QMutexLocker ml(&l);
		return busy;
	}

	bool process(const RawBuffer &buf)
	{
		if (isBusy())
			return false;
		markBusy(true);
		currentBuf = buf;
		wsem.release();
		return true;
	}

	void stop()
	{
		QMutexLocker ml(&l);
		active = false;
		wsem.release();
	}

	BaseLmmElement *el;
	JobDistributorElement *d;
protected:
	void doJob()
	{
		el->process(0, currentBuf);
		d->finishBuffer(currentBuf);
	}

	void markBusy(bool v)
	{
		QMutexLocker ml(&l);
		busy = v;
	}

	bool busy;
	QMutex l;
	QSemaphore wsem;
	bool active;
	RawBuffer currentBuf;
};

class ReduceThread : public LmmThread
{
public:
	ReduceThread()
		: LmmThread("SomeReduceThread")
	{

	}

	ElementIOQueue *q;
	JobDistributorElement *d;
	int operation()
	{
		RawBuffer buf = q->getBuffer(NULL);
		return d->newOutputBuffer(0, buf);
	}
};

JobDistributorElement::JobDistributorElement(QObject *parent)
	: BaseLmmElement(parent)
{
	reducer = new ReduceThread;
	reducer->d = this;
	reducer->q = new ElementIOQueue;
	method = DIST_SEQUENTIAL;
}

int JobDistributorElement::addElement(BaseLmmElement *el)
{
	DistHandlerThread *handler = new DistHandlerThread;
	handler->el = el;
	handler->d = this;
	free << handler;
	el->addOutputQueue(reducer->q);
	return 0;
}

void JobDistributorElement::setMethod(JobDistributorElement::DistributionMethod m)
{
	method = m;
}

int JobDistributorElement::elementCount()
{
	return free.size();
}

int JobDistributorElement::start()
{
	reducer->start();
	foreach (DistHandlerThread *h, free) {
		h->start();
		h->el->setStreamDuration(-1);
		h->el->setStreamTime(streamTime);
		h->el->start();
	}
	return BaseLmmElement::start();
}

int JobDistributorElement::stop()
{
	reducer->stop();
	free << busy.values();
	foreach (DistHandlerThread *h, free)
		h->stop();
	return BaseLmmElement::stop();
}

int JobDistributorElement::finishBuffer(const RawBuffer &buf)
{
	mInfo("buffer no=%d, method", buf.constPars()->streamBufferNo, method);
	if (method == DIST_ALL)
		return 0;
	dlock.lock();
	if (busy.contains(buf.constPars()->streamBufferNo)) {
		DistHandlerThread *handler = busy[buf.constPars()->streamBufferNo];
		busy.remove(buf.constPars()->streamBufferNo);
		free << handler;
	} else
		ffDebug() << "oh shite" << buf.constPars()->streamBufferNo;
	dlock.unlock();
	return 0;
}

int JobDistributorElement::processBuffer(const RawBuffer &buf)
{
	mInfo("buffer no=%d with size %d, free handlers %d", buf.constPars()->streamBufferNo, buf.size(), free.size());
	if (method == DIST_SEQUENTIAL) {
		dlock.lock();
		DistHandlerThread *handler = NULL;
		if (free.size())
			handler = free.takeFirst();
		while (handler == NULL || !handler->process(buf)) {
			dlock.unlock();
			usleep(1000);
			dlock.lock();
			if (handler == NULL && free.size())
				handler = free.takeFirst();
		}
		busy.insert(buf.constPars()->streamBufferNo, handler);
		dlock.unlock();
	} else if (method == DIST_ALL) {
		for (int i = 0; i < free.size(); i++) {
			DistHandlerThread *h = free[i];
			while (!h->process(buf))
				usleep(100);
		}
		for (int i = 0; i < free.size(); i++) {
			while (free[i]->isBusy())
				usleep(100);
		}
	}
	return 0;
}

