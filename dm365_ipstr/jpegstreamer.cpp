#include "jpegstreamer.h"
#include "jpegshotserver.h"

#include <lmm/debug.h>
#include <lmm/dm365/dm365camerainput.h>
#include <lmm/bufferqueue.h>
#include <lmm/dmai/jpegencoder.h>
#include <lmm/baselmmpipeline.h>

JpegStreamer::JpegStreamer(QObject *parent)
	: BaseStreamer(parent)
{
	DM365CameraInput *camIn = new DM365CameraInput;
	camIn->setSize(0, QSize(1920, 1080));
	JpegEncoder *jpeg = new JpegEncoder;
	jpeg->setParameter("videoWidth", 1920);
	jpeg->setParameter("videoHeight", 1080);
	jpeg->setQualityFactor(70);
	que1 = new BufferQueue;
	que1->setQueueSize(10);

	BaseLmmPipeline *p1 = addPipeline();
	p1->append(camIn);
	p1->append(jpeg);
	p1->append(que1);
	p1->end();

	BaseLmmPipeline *p2 = addPipeline();
	p2->append(camIn);
	p2->end();

	JpegShotServer *server = new JpegShotServer(this, 4571);
}

QList<RawBuffer> JpegStreamer::getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount)
{
	Q_UNUSED(ch);
	Q_UNUSED(codec);
	Q_UNUSED(ts);
	return que1->getLast(frameCount);
}

int JpegStreamer::pipelineOutput(BaseLmmPipeline *p, const RawBuffer &buf)
{
	if (p == getPipeline(0))
		ffDebug() << buf.size();
	return 0;
}

