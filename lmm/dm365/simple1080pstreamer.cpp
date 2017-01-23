#include "simple1080pstreamer.h"

#include <lmm/debug.h>
#include <lmm/bufferqueue.h>
#include <lmm/textoverlay.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/dmai/jpegencoder.h>
#include <lmm/dmai/h264encoder.h>
#include <lmm/dm365/dm365dmacopy.h>
#include <lmm/rtp/rtptransmitter.h>
#include <lmm/rtsp/basertspserver.h>
#include <lmm/dm365/dm365camerainput.h>
#include <lmm/dm365/dm365videooutput.h>
#include <lmm/pipeline/basepipeelement.h>

static inline void setElSize(BaseLmmElement *el, QSize sz)
{
	el->setParameter("videoWidth", sz.width());
	el->setParameter("videoHeight", sz.height());
}

Simple1080pStreamer::Simple1080pStreamer(QObject *parent) :
	BaseStreamer(parent),
	sensorMode(false)
{
	camIn = new DM365CameraInput;
	camIn->setSize(0, QSize(1920, 1080));

	int videoFps = 25;
	QSize sz0 = camIn->getSize(0);
	//QSize sz1 = camIn->getSize(1);

	enc264High = new H264Encoder;
	enc264High->setFrameRate(videoFps);
	enc264High->setSeiEnabled(false);
	enc264High->setPacketized(true);
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
	//enc264High->setBitrate(10000000);
	/*
	 *  Profile 2 is derived from TI recommendations for video surveillance
	 */
	enc264High->setProfile(0);
	setElSize(enc264High, sz0);

	//seiInserterHigh = new SeiInserter(enc264High);
	//seiInserterHigh->setObjectName("SeiInserterHigh");
	//elements << seiInserterHigh;

	/* overlay element, only for High res channel */
	/*textOverlay = new TextOverlay(TextOverlay::PIXEL_MAP);
	textOverlay->setObjectName("TextOverlay");
	setElSize(textOverlay, sz0);
	elements << textOverlay;*/

	RtpTransmitter *rtpHigh = new RtpTransmitter(this);

	/* setup pipelines */
	BaseLmmPipeline *p1 = addPipeline();
	/* construct pipeline1 */
	p1->append(camIn);
	//p1->appendPipe(textOverlay)->setPassThru(true);
	p1->append(enc264High);
	p1->append(rtpHigh);
	p1->end();
	/*
	 * If encoder is not packetized, we need to copy buffers,
	 * otherwise we may optimize a one more memcopy out
	 */
	//p1->appendPipe(rtsp)->setCopyOnUse(!enc264High->isPacketized());

	/* construct pipeline2 */
	BaseLmmPipeline *p2 = addPipeline();
	p2->append(camIn);
	p2->end();

	/* rtsp server */
	BaseRtspServer *rtsp = new BaseRtspServer(this);
	rtsp->addStream("stream1", false, rtpHigh);
	rtsp->addStream("stream1m", true, rtpHigh, 15678);
}

QList<RawBuffer> Simple1080pStreamer::getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount)
{
	Q_UNUSED(ch);
	Q_UNUSED(codec);
	Q_UNUSED(ts);
	Q_UNUSED(frameCount);
	return QList<RawBuffer>();
}

int Simple1080pStreamer::startStreamer()
{
	return 0;
}

int Simple1080pStreamer::pipelineOutput(BaseLmmPipeline *pipeline, const RawBuffer &buf)
{
	Q_UNUSED(buf);
	Q_UNUSED(pipeline);
	return 0;
}

QSize Simple1080pStreamer::getInputSize(int input)
{
	return camIn->getSize(input);
}
