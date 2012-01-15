#include "avidecoder.h"
#include "avidemux.h"
#include "mad.h"
#include "rawbuffer.h"
#include "emdesk/debug.h"

#include <QTime>
#include <QTimer>
#include <QThread>
#include <QtConcurrentRun>

/*class DemuxThread : public QThread
{
public:
	DemuxThread()
	{
		demux = new AviDemux();
	}
	AviDemux * getDemux() { return demux; }
	void run()
	{
		QTimer::singleShot(0, demux, SLOT(decode()));
		exec();
	}
private:
	AviDemux *demux;
};*/

AviDecoder::AviDecoder(QObject *parent) :
	QObject(parent)
{
#if 0
	//DemuxThread *demuxThread = new DemuxThread();
	demux = new AviDemux;//demuxThread->getDemux();
	int err = demux->setSource("/media/net/Fringe.S04E06.HDTV.XviD-LOL.[VTV].avi");
	if (err)
		return;
	connect(demux, SIGNAL(newAudioFrame()), SLOT(newAudioFrame()), Qt::QueuedConnection);
	QThread *demuxThread = new QThread;
	demux->moveToThread(demuxThread);
	demuxThread->start();
	QTimer::singleShot(0, demux, SLOT(decode()));
#endif
}

int AviDecoder::startDecoding()
{
	streamTime = new QTime;
	audioDecoder = new Mad;
	demux = new AviDemux;
	int err = demux->setSource("/media/net/Fringe.S04E06.HDTV.XviD-LOL.[VTV].avi");
	if (err)
		return err;
	connect(demux, SIGNAL(newAudioFrame()), SLOT(newAudioFrame()), Qt::QueuedConnection);

#if 1
	timer = new QTimer(this);
	timer->setSingleShot(true);
	connect(timer, SIGNAL(timeout()), this, SLOT(decodeLoop()));
	timer->start(10);
	streamTime->start();
#endif
	//future = QtConcurrent::run(demux, &AviDemux::decode);

	return 0;
}

int AviDecoder::getDuration()
{
	return demux->getTotalDuration();
}

void AviDecoder::newAudioFrame()
{
	qDebug() << demux->getCurrentPosition() << demux->getTotalDuration();
	//timer->start(10);
	return;
	RawBuffer *buf = demux->nextAudioBuffer();
	if (buf)
		qDebug() << buf->getDuration();
	//if (future->isFinished())
	//qDebug() << "decoding finished";
}

void AviDecoder::decodeLoop()
{
	demux->demuxOne();
	audioLoop();
	//qDebug() << demux->getCurrentPosition() / 1000000 << demux->getTotalDuration() / 1000000;
	timer->start(10);
}

void AviDecoder::audioLoop()
{
	RawBuffer *buf = demux->nextAudioBuffer();
	if (buf) {
		//qDebug() << buf->getDuration() << buf->size();
		audioDecoder->addBuffer(buf);
	}
	audioDecoder->decode();

	buf = audioDecoder->nextBuffer();
	if (buf) {
		qDebug() << streamTime->elapsed() << "decoded: " << buf->size() << buf;
		delete buf;
	}
}
