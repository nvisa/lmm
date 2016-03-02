#include "basestreamer.h"

#include <lmm/streamtime.h>
#include <lmm/debug.h>
#include <lmm/tools/cpuload.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/rtsp/basertspserver.h>

#include <errno.h>
#include <signal.h>

#include <QDir>
#include <QFile>
#include <QSize>
#include <QCoreApplication>

static inline void setElSize(BaseLmmElement *el, QSize sz)
{
	el->setParameter("videoWidth", sz.width());
	el->setParameter("videoHeight", sz.height());
}

BaseStreamer::BaseStreamer(QObject *parent) :
	PipelineManager(parent)
{
	/* ensure CpuLoad is started in correct thread context */
	CpuLoad::getCpuLoad();
}

QList<RawBuffer> BaseStreamer::getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount)
{
	Q_UNUSED(ch);
	Q_UNUSED(codec);
	Q_UNUSED(ts);
	Q_UNUSED(frameCount);
	return QList<RawBuffer>();
}

QSize BaseStreamer::getInputSize(int input)
{
	Q_UNUSED(input);
	return QSize(0, 0);
}

int BaseStreamer::start()
{
	/*float fpsOut = BaseSettingHandler::getSetting("camera_device.output_fps").toFloat();
	if (fpsOut > 0) {
		const QList<RtpInterface *> muxers = rtsp->getMuxers();
		foreach (RtpInterface *muxer, muxers) {
			muxer->setFrameRate(fpsOut);
		}
	}*/

	return PipelineManager::start();
}

int BaseStreamer::stop()
{
	return PipelineManager::stop();
}

void BaseStreamer::signalReceived(int sig)
{
	Q_UNUSED(sig);
	stop();
	QCoreApplication::exit();
}
