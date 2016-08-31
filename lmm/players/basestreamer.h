#ifndef BASESTREAMER_H
#define BASESTREAMER_H

#include <lmm/baseplayer.h>
#include <lmm/lmmcommon.h>
#include <lmm/lmmthread.h>
#include <lmm/pipeline/pipelinemanager.h>

#include <QHash>
#include <QTimer>

class BaseRtspServer;
class BaseLmmPipeline;

class BaseStreamer : public PipelineManager
{
	Q_OBJECT
public:
	explicit BaseStreamer(QObject *parent = 0);

	virtual int start();
	virtual int stop();

	virtual QList<RawBuffer> getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount);
signals:
	
protected slots:

protected:
	BaseRtspServer *rtsp;
	QList<BaseLmmElement *> elements;

	void signalReceived(int sig);
};

#endif // BASESTREAMER_H
