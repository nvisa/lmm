#include "avidecoder.h"
#include "avidemux.h"
#include "mad.h"
#include "rawbuffer.h"
#include "alsaoutput.h"
#include "fboutput.h"
#include "dmaidecoder.h"
#include "emdesk/debug.h"
#include "emdesk/hardwareoperations.h"

#include <QTime>
#include <QTimer>
#include <QThread>

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
	streamTime = new QTime;
	audioDecoder = new Mad;
	videoDecoder = new DmaiDecoder;
	audioOutput = new AlsaOutput;
	videoOutput = new FbOutput;
	demux = new AviDemux;

	videoOutput->setStreamTime(streamTime);
}

AviDecoder::~AviDecoder()
{
	if (videoDecoder) {
		delete videoDecoder;
		delete audioDecoder;
		delete videoDecoder;
		delete audioOutput;
		delete videoOutput;
		delete demux;
		videoDecoder = NULL;
	}
}

int AviDecoder::startDecoding()
{
	state = RUNNING;
	int err = demux->setSource("/media/net/Fringe.S04E06.HDTV.XviD-LOL.[VTV].avi");
	if (err)
		return err;
	connect(demux, SIGNAL(newAudioFrame()), SLOT(newAudioFrame()), Qt::QueuedConnection);
	HardwareOperations::blendOSD(true, 31);
	videoDecoder->start();
	timer = new QTimer(this);
	timer->setSingleShot(true);
	connect(timer, SIGNAL(timeout()), this, SLOT(decodeLoop()));
	timer->start(10);
	streamTime->start();
	videoOutput->setStreamDuration(demux->getTotalDuration());

	return 0;
}

void AviDecoder::stopDecoding()
{
	state = STOPPED;
	videoDecoder->stop();
	HardwareOperations::blendOSD(false);
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
	if (state != RUNNING) {
		mDebug("we are not in a running state");
		return;
	}
	demux->demuxOne();
	//audioLoop();
	videoLoop();
	timer->start(5);
}

void AviDecoder::audioLoop()
{
	RawBuffer *buf = demux->nextAudioBuffer();
	if (buf) {
		audioDecoder->addBuffer(buf);
	}
	audioDecoder->decodeAll();
	buf = audioDecoder->nextBuffer();
	if (buf) {
		qDebug() << streamTime->elapsed() << "decoded: " << buf->size() << buf;
		audioOutput->addBuffer(buf);
	}
	audioOutput->output();
}

void AviDecoder::videoLoop()
{
	RawBuffer *buf = demux->nextVideoBuffer();
	if (buf) {
		videoDecoder->addBuffer(buf);
	}
	videoDecoder->decodeOne();
	buf = videoDecoder->nextBuffer();
	if (buf) {
		videoOutput->addBuffer(buf);
	}
	videoOutput->output();
}
