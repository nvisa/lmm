#include "pipelinemanager.h"

#include "debug.h"
#include "lmmcommon.h"
#include "baseplayer.h"
#include "baselmmpipeline.h"
#include "pipelinedebugger.h"

#include <errno.h>
#include <signal.h>

#if QT_VERSION > 0x050000
#include "qtvideooutput.h"
#endif
#include "ffmpeg/baselmmdemux.h"
#include "lmm/ffmpeg/ffmpegdecoder.h"
#include "lmm/ffmpeg/ffmpegcolorspace.h"
#include "lmm/jobdistributorelement.h"
#include "lmm/videoscaler.h"
#include "lmm/x264encoder.h"

#if QT_VERSION > 0x050000
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#endif
#include <QElapsedTimer>
#include <QCoreApplication>

extern __thread LmmThread *currentLmmThread;
static __thread BaseLmmPipeline *currentPipeline = NULL;

PipelineManager::PipelineManager(QObject *parent) :
	BaseLmmElement(parent)
{
	debug = 0;
	quitting = false;
	dbg = PipelineDebugger::GetInstance();
	QTimer::singleShot(1000, this, SLOT(timeout()));
	checkPipelineWdts = false;
}

int PipelineManager::start()
{
	quitting = false;
	for (int i = 0; i < pipelines.size(); i++) {
		int err = startPipeline(i);
		if (err)
			return err;
	}

	return BaseLmmElement::start();
}

int PipelineManager::stop()
{
	quitting = true;
	for (int i = 0; i < pipelines.size(); i++)
		pipelines[i]->stop();
	for (int i = 0; i < pipelines.size(); i++)
		pipelines[i]->waitForFinished(100);
	for (int i = 0; i < pipelines.size(); i++)
		dbg->removePipeline(pipelines[i]);
	return BaseLmmElement::stop();
}

int PipelineManager::flush()
{
	for (int i = 0; i < pipelines.size(); i++)
		pipelines[i]->flush();
	return BaseLmmElement::flush();
}

int PipelineManager::getPipelineCount()
{
	return pipelines.size();
}

BaseLmmPipeline *PipelineManager::getPipeline(int ind)
{
	return pipelines[ind];
}

#if QT_VERSION > 0x050000
static QHash<QString, elementCreateFactory> registeredElements;
static BaseLmmElement * createElement(const QJsonObject &elobj)
{
	QString elType = elobj["type"].toString();
	BaseLmmElement *el = NULL;
	if (elType == "demux") {
		BaseLmmDemux *demux = new BaseLmmDemux;
		el = demux;
		demux->setAudioDemuxing(elobj["audio"].toBool());
		demux->setSource(elobj["source"].toString());
	} else if (elType == "decoder") {
		FFmpegDecoder *dec = new FFmpegDecoder;
		el = dec;
		dec->setVideoResolution(elobj["width"].toInt(), elobj["height"].toInt());
		dec->setH264NalChecking(elobj["h264NalChecking"].toBool());
		dec->setBufferCount(elobj["bufferCount"].toInt());
	} else if (elType == "colorspace") {
		FFmpegColorSpace *cp = new FFmpegColorSpace;
		el = cp;
		cp->setBufferCount(elobj["bufferCount"].toInt());
		cp->setOutputFormat(FFmpegColorSpace::getFormat(elobj["outputFormat"].toString()));
		cp->setInputFormat(FFmpegColorSpace::getFormat(elobj["inputFormat"].toString()));
	} else if (elType == "jobdistro") {
		JobDistributorElement *dist = new JobDistributorElement;
		dist->setMethod(JobDistributorElement::DistributionMethod(elobj["method"].toInt()));
		el = dist;
		int repeat = elobj["repeat"].toInt();
		for (int i = 0; i < repeat; i++) {
			foreach (QJsonValue v, elobj["elements"].toArray()) {
				QJsonObject subel = v.toObject();
				BaseLmmElement *sel = createElement(subel);
				if (sel == NULL) {
					fDebug("un-supported element '%s' in jobdistro definition", qPrintable(subel["type"].toString()));
					return NULL;
				}
				dist->addElement(sel);
			}
		}
	} else if (elType == "videoOutput") {
		QtVideoOutput *vout = new QtVideoOutput;
		el = vout;
	} else if (elType == "videoScaler") {
		VideoScaler *sc = new VideoScaler;
		sc->setOutputResolution(elobj["width"].toInt(), elobj["height"].toInt());
		sc->setMode(elobj["mode"].toInt());
		el = sc;
	} else if (elType == "videoEncoder") {
		x264Encoder *enc = new x264Encoder;
		enc->setVideoResolution(QSize(elobj["width"].toInt(), elobj["height"].toInt()));
		enc->setBitrate(elobj["bitrate"].toInt());
		enc->setThreadCount(elobj["threads"].toInt());
		enc->setPreset(elobj["preset"].toString());
		el = enc;
	} else if (registeredElements.contains(elType))
		return (*(registeredElements[elType]))(elobj);
	return el;
}

int PipelineManager::parseConfig(const QString &filename)
{
	QFile f(filename);
	f.open(QIODevice::ReadOnly);
	QJsonObject tobj = QJsonDocument::fromJson(f.readAll()).object();
	f.close();

	debug = tobj["debug"].toInt();

	QJsonArray pipelines = tobj["pipelines"].toArray();
	foreach (QJsonValue v, pipelines) {
		QJsonObject pipeline = v.toObject();
		QJsonArray elements = pipeline["elements"].toArray();
		QList<BaseLmmElement *> els;
		foreach (QJsonValue v2, elements) {
			QJsonObject elobj = v2.toObject();
			if (elobj["disabled"].toBool())
				continue;

			QString elType = elobj["type"].toString();
			BaseLmmElement *el = createElement(elobj);
			if (!el) {
				mDebug("un-supported element '%s' in pipeline definition", qPrintable(elType));
				return -EINVAL;
			}
			els << el;
		}

		BaseLmmPipeline *pl = addPipeline();
		for (int j = 0; j < els.size(); j++)
			pl->append(els[j]);
		//pl->setMaxTimeout(pipelineMaxTimeout);
		pl->end();
		pl->setQuitOnThreadError(pipeline["quitOnThreadError"].toBool());

		//postInitPipeline(pl);
	}

	return 0;
}

void PipelineManager::registerElementFactory(const QString &type, elementCreateFactory factory)
{
	registeredElements.insert(type, factory);
}
#else
int PipelineManager::parseConfig(const QString &filename)
{
	Q_UNUSED(filename);
	return -1;
}
#endif

void PipelineManager::timeout()
{
	if (checkPipelineWdts) {
		BaseLmmPipeline *failed = NULL;
		pipelineLock.lock();
		QHashIterator<BaseLmmPipeline *, QElapsedTimer *> hi(pipelineWdts);
		while (hi.hasNext()) {
			hi.next();
			if (hi.key()->getMaxTimeout() && hi.value()->elapsed() > hi.key()->getMaxTimeout()) {
				failed = hi.key();
				break;
			}
		}
		pipelineLock.unlock();
		if (failed) {
			mDebug("pipeline '%s' failed, quitting from application", qPrintable(failed->objectName()));
			QCoreApplication::instance()->exit(234);
		}
	}

	if (!dbg->isStarted())
		dbg->start();

	QTimer::singleShot(1000, this, SLOT(timeout()));
}

void PipelineManager::signalReceived(int sig)
{
	Q_UNUSED(sig);
}

int PipelineManager::processBuffer(const RawBuffer &buf)
{
	Q_UNUSED(buf);
	return 0;
}

BaseLmmPipeline *PipelineManager::addPipeline()
{
	BaseLmmPipeline *pipeline = new BaseLmmPipeline(this);
	pipeline->setObjectName(QString("Pipeline%1").arg(pipelines.size()));
	pipelines << pipeline;
	pipelineWdts.insert(pipeline, new QElapsedTimer);
	pipelineWdts[pipeline]->start();
	dbg->addPipeline(pipeline);
	connect(pipeline, SIGNAL(playbackFinished()), SLOT(pipelineFinished()));
	return pipeline;
}

int PipelineManager::pipelineOutput(BaseLmmPipeline *p, const RawBuffer &buf)
{
	if (debug)
		ffDebug() << p << buf.constPars()->streamBufferNo << buf.size() << p->getOutputQueue(0)->getFps();
	return 0;
}

QList<BaseLmmElement *> PipelineManager::getElements()
{
	QList<BaseLmmElement *> list;
	for (int i = 0; i < pipelines.size(); i++) {
		int cnt = pipelines[i]->getPipeCount();
		for (int j = 0; j < cnt; j++)
			if (!list.contains(pipelines[i]->getPipe(j)))
				list << pipelines[i]->getPipe(j);
	}
	return list;
}

int PipelineManager::startPipeline(int i)
{
	int err = pipelines[i]->start();
	if (err)
		return err;

	QString name = QString("MainPipelineThread%1").arg(i + 1);
	pipelineLock.lock();
	pipelinesByThread.insert(name, pipelines[i]);
	pipelineLock.unlock();
	createOpThread(&PipelineManager::pipelineThread, name, PipelineManager);
	return 0;
}

int PipelineManager::pipelineThread()
{
	if (quitting)
		return -EINVAL;
	if (!currentPipeline) {
		pipelineLock.lock();
		currentPipeline = pipelinesByThread[currentLmmThread->threadName()];
		pipelineLock.unlock();
	}
	RawBuffer buf = currentPipeline->nextBufferBlocking(0);
	if (buf.size())
		currentPipeline->updateStats(buf);
	pipelineLock.lock();
	pipelineWdts[currentPipeline]->restart();
	pipelineLock.unlock();
	return pipelineOutput(currentPipeline, buf);
}
