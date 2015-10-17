#include "baselmmpipeline.h"
#include "baseplayer.h"
#include "debug.h"
#include "streamtime.h"

#include <QEventLoop>

#include <errno.h>

/**
	\class BaseLmmPipeline

	\brief Bu sinif LMM pipeline mimarisinin temelini olusturur.

	Pipeline yaratildiktan sonra appendPipe() fonksiyonu ile yeni
	bir elemeni ekleyebilirsiniz. appendPipe() fonskyonu parametre
	olarak gecilen elemani en son olarak eklenmis elemanin ciktisini
	alacak seklide ekler. Eger kendisinden once bir eleman yoksa eleman
	ilk eleman haline gelir. Eklenen elemanin ciktisi pipeline'nin kendisine
	baglanir.

	addPipe() fonksiyonu ile pipeline'a rastgele bir noktadan bir eleman
	ekleyebilirsiniz. addPipe() ile eklenen elemanin girisine baska bir eleman
	baglanmaz, bu baglantiyi elinizle yapmak zorundasinizdir. Isteye bagli olarak
	bir sonraki elemani belirtebilirsiniz. Bu istege bagli baglantilari yapmak
	icin BasePipeElement::setPipe() fonksiyonunu kullanabilirsiniz.

	\section Hata Ayiklama Mekanizmasi



	\ingroup lmm
*/

BaseLmmPipeline::BaseLmmPipeline(QObject *parent) :
	BaseLmmElement(parent),
	pipelineReady(false)
{
	el = new QEventLoop(this);
	pipelineReady = false;
	streamTime = new StreamTime;
}

BaseLmmPipeline::~BaseLmmPipeline()
{
	if (threads.size()) {
		mDebug("destroying threads");
		qDeleteAll(threads);
		threads.clear();
	}
}

int BaseLmmPipeline::start()
{
	if (threads.size()) {
		mDebug("destroying threads");
		qDeleteAll(threads);
		threads.clear();
	}

	finishedThreadCount = 0;

	for (int i = 0; i < pipesNew.size(); i++) {
		/* start element */
		BaseLmmElement *el = pipesNew[i];
		mDebug("starting element %s(%s)", el->metaObject()->className(), qPrintable(el->objectName()));
		el->flush();
		el->setStreamTime(streamTime);
		el->setStreamDuration(-1);
		/* start element */
		int err = el->start();
		if (err) {
			mDebug("error %d starting element %s", err, el->metaObject()->className());
			return err;
		}

		/* create element threads */
		QString desc = el->objectName();
		if (desc.isEmpty())
			desc = el->metaObject()->className();
		int incnt = el->getInputQueueCount();
		for (int j = 0; j < incnt; j++) {
			LmmThread *th = new OpThread2<BaseLmmElement>(el, &BaseLmmElement::processBlocking,
														  objectName().append("P%1%2").arg(j).arg(desc), 0);
			threads.insert(th->threadName(), th);
			th->start(QThread::LowestPriority);
		}
	}

	return BaseLmmElement::start();
}

int BaseLmmPipeline::stop()
{
	return BaseLmmElement::stop();
}

int BaseLmmPipeline::getPipeCount()
{
	return pipesNew.size();
}

BaseLmmElement *BaseLmmPipeline::getPipe(int off)
{
	return pipesNew[off];
}

void BaseLmmPipeline::setPipelineReady(bool v)
{
	pipelineReady = v;
}

bool BaseLmmPipeline::isPipelineReady()
{
	return pipelineReady;
}

const QList<LmmThread *> BaseLmmPipeline::getThreads()
{
	return threads.values();
}

int BaseLmmPipeline::append(BaseLmmElement *el)
{
	if (pipesNew.size()) {
		if (!el->getInputQueueCount())
			el->addInputQueue(el->createIOQueue());
		BaseLmmElement *last = pipesNew.last();
		last->addOutputQueue(el->getInputQueue(0));
	}
	pipesNew << el;
	return 0;
}

int BaseLmmPipeline::processPipeline()
{
	return processBlocking();
}

int BaseLmmPipeline::checkPipelineOutput()
{
	return processBlocking(1);
}

void BaseLmmPipeline::waitToFinish()
{
	thLock.lock();
	if (finishedThreadCount != threads.size()) {
		thLock.unlock();
		el->exec();
		return;
	}
	thLock.unlock();
}

void BaseLmmPipeline::threadFinished(LmmThread *)
{
	thLock.lock();
	mDebug("thread finished: %d %d", finishedThreadCount, threads.size());
	if (++finishedThreadCount == threads.size()) {
		el->quit();
	}
	thLock.unlock();
}

int BaseLmmPipeline::processBuffer(const RawBuffer &buf)
{
	return -ENOENT;
}

int BaseLmmPipeline::processBuffer(int ch, const RawBuffer &buf)
{
	Q_UNUSED(ch);
	/* we have new buffer output from pipeline */
	return newOutputBuffer(0, buf);
}
