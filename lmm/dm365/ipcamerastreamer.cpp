#include "ipcamerastreamer.h"

#include <lmm/debug.h>
#include <lmm/bufferqueue.h>
#include <lmm/textoverlay.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/dmai/jpegencoder.h>
#include <lmm/dmai/h264encoder.h>
#include <lmm/dmai/seiinserter.h>
#include <lmm/rtp/rtptransmitter.h>
#include <lmm/dm365/dm365dmacopy.h>
#include <lmm/rtsp/basertspserver.h>
#include <lmm/dm365/dm365camerainput.h>
#include <lmm/dm365/dm365videooutput.h>
#include <lmm/pipeline/basepipeelement.h>

#include <QFile>
#include <QDateTime>

#include <errno.h>

static inline void setElSize(BaseLmmElement *el, QSize sz)
{
	el->setParameter("videoWidth", sz.width());
	el->setParameter("videoHeight", sz.height());
}

class ImplementationSettings {
public:
	ImplementationSettings()
	{
		rawQueueSize = 2;
		rawQueueSize2 = 2;
		h264QueueSize = 2;
		h264QueueAllocSize = 1024 * 300;
		clonerBufferCount = 20;
	}

	int clonerBufferCount;
	int h264QueueSize;
	int h264QueueAllocSize;
	int rawQueueSize;
	int rawQueueSize2;
};

#define setIntFromIni(__key, t) \
	if (_sets.contains(__key)) \
		t = _sets[__key].toInt();

IPCameraStreamer::IPCameraStreamer(QObject *parent) :
	BaseStreamer(parent)
{
	ImplementationSettings sets;
#if 0
	Q_UNUSED(sets);
	/* set-up defaults */
	setIntFromIni("h264_queue_size", sets.h264QueueSize);
	setIntFromIni("h264_queue_alloc_size", sets.h264QueueAllocSize);
	setIntFromIni("raw_queue_size", sets.rawQueueSize);
	setIntFromIni("raw_queue_size2", sets.rawQueueSize2);
#endif

	camIn = new DM365CameraInput();
	int videoFps = 30;
	QSize sz0 = camIn->getSize(0);
	QSize sz1 = camIn->getSize(1);
	BaseLmmPipeline *p1 = addPipeline();

	/* overlay element, only for High res channel */
	textOverlay = new TextOverlay(TextOverlay::PIXEL_MAP);
	textOverlay->setObjectName("TextOverlay");
	setElSize(textOverlay, sz0);

	enc264High = new H264Encoder;
	enc264High->setSeiEnabled(true);
	enc264High->setPacketized(false);
	enc264High->setObjectName("H264EncoderHigh");
	/*
	 *  We set bitrate according to suggestion presented at [1].
	 *  For 1280x720@30fps this results something like 6 mbit.
	 *  For 320x240@20fps this results something like 500 kbit.
	 *
	 *  We take motion rate as 3 which can be something between 1-4.
	 *
	 * [1] http://stackoverflow.com/questions/5024114/suggested-compression-ratio-with-h-264
	 */
	enc264High->setBitrateControl(DmaiEncoder::RATE_VBR);
	enc264High->setBitrate(sz0.width() * sz0.height() * videoFps * 3 * 0.07);
	/*
	 *  Profile 2 is derived from TI recommendations for video surveillance
	 */
	enc264High->setProfile(2);
	enc264High->setBufferCount(sets.h264QueueSize + 10);
	setElSize(enc264High, sz0);

	seiInserterHigh = new SeiInserter(enc264High);
	seiInserterHigh->setObjectName("SeiInserterHigh");

	cloner = new DM365DmaCopy(this, 1);
	cloner->setBufferCount(sets.clonerBufferCount);
	cloner->setAllocateSize(sets.h264QueueAllocSize);

	h264Queue = new BufferQueue(this);
	h264Queue->setQueueSize(sets.h264QueueSize);
	rawQueue = new BufferQueue(this);
	rawQueue->setQueueSize(sets.rawQueueSize);

	rtpHigh = new RtpTransmitter(this);

	p1->append(camIn);
	p1->append(textOverlay);
	p1->append(rawQueue);
	p1->append(enc264High);
	p1->append(seiInserterHigh);
	p1->append(cloner);
	p1->append(h264Queue);
	p1->append(rtpHigh);
	p1->end();

	/* pipeline 2 */
	enc264Low = new H264Encoder;
	enc264Low->setSeiEnabled(false);
	enc264Low->setPacketized(true);
	enc264Low->setObjectName("H264EncoderLow");
	enc264Low->setBitrateControl(DmaiEncoder::RATE_VBR);
	enc264Low->setBitrate(sz1.width() * sz1.height() * videoFps * 3 * 0.07);
	enc264Low->setProfile(0);
	setElSize(enc264Low, sz1);

	rawQueue2 = new BufferQueue(this);
	rawQueue2->setQueueSize(sets.rawQueueSize2);

	rtpLow = new RtpTransmitter(this);

	BaseLmmPipeline *p2 = addPipeline();
	p2->append(camIn);
	p2->append(rawQueue2);
	p2->append(enc264Low);
	p2->append(rtpLow);
	p2->end();

	/* rtsp server */
	BaseRtspServer *rtsp = new BaseRtspServer(this);
	rtsp->addStream("stream1", false, rtpHigh);
	rtsp->addStream("stream1m", true, rtpHigh, 15678);
	rtsp->addStream("stream2", false, rtpLow);
	rtsp->addStream("stream2m", true, rtpLow, 15688);
}

QList<RawBuffer> IPCameraStreamer::getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount)
{
	if (codec == Lmm::CODEC_H264) {
		QList<RawBuffer> bufs;
		if (ts)
			bufs = h264Queue->findBuffers(ts, frameCount);
		else
			bufs = h264Queue->getLast(frameCount);
		return bufs;
	}

	/* raw or jpeg snapshot */
	JpegEncoder *enc = encJpegHigh;
	BufferQueue *queue = rawQueue;
	if (ch == 1) {
		queue = rawQueue2;
		enc = encJpegLow;
	}
	QList<RawBuffer> bufs;
	if (ts)
		bufs = queue->findBuffers(ts, frameCount);
	else
		bufs = queue->getLast(frameCount);
	if (codec == Lmm::CODEC_RAW)
		return bufs;
	QList<RawBuffer> bufs2;
	/* we use while loop so that encoded buffers can be released */
	while (bufs.size()) {
		enc->addBuffer(0, bufs.takeFirst());
		enc->processBlocking();
		bufs2 << enc->nextBufferBlocking(0);
	}
	return bufs2;
}

int IPCameraStreamer::startStreamer()
{
	return 0;
}

int IPCameraStreamer::pipelineOutput(BaseLmmPipeline *pipeline, const RawBuffer &buf)
{
	Q_UNUSED(buf);
	Q_UNUSED(pipeline);
	return 0;
}

QSize IPCameraStreamer::getInputSize(int input)
{
	return camIn->getSize(input);
}
