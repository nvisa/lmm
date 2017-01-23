#include "simplertpstreamer.h"

#include <lmm/debug.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/dmai/h264encoder.h>
#include <lmm/rtp/rtppacketizer.h>
#include <lmm/rtsp/basertspserver.h>
#include <lmm/dm365/dm365camerainput.h>
#include <lmm/pipeline/basepipeelement.h>
#include <lmm/fileoutput.h>

static inline void setElSize(BaseLmmElement *el, QSize sz)
{
	el->setParameter("videoWidth", sz.width());
	el->setParameter("videoHeight", sz.height());
}

SimpleRtpStreamer::SimpleRtpStreamer(const QString &dstIp, QObject *parent) :
	BaseStreamer(parent)
{
	targetIp = dstIp;
	camIn = new DM365CameraInput;

	int videoFps = 30;
	QSize sz0 = camIn->getSize(0);
	QSize sz1 = camIn->getSize(1);

	enc264High = new H264Encoder;
	enc264High->setSeiEnabled(true);
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
	/*
	 *  Profile 2 is derived from TI recommendations for video surveillance
	 */
	enc264High->setProfile(2);
	setElSize(enc264High, sz0);

	//seiInserterHigh = new SeiInserter(enc264High);
	//seiInserterHigh->setObjectName("SeiInserterHigh");

	//rtp = rtsp->getMuxers().first();
	//rtp->setDestinationIpAddress(ip);
	rtp->setDestinationDataPort(31546);
	rtp->setDestinationControlPort(31547);
	rtp->setSsrc(0x78457845);
	rtp->setSourceDataPort(0);
	rtp->setSourceControlPort(0);

	fout = new FileOutput;
	fout->setFileName("ref.h264");
}

int SimpleRtpStreamer::start()
{
	QString ip = targetIp;
	if (ip.isEmpty())
		ip = "192.168.2.2";
	ffDebug() << "*****************************" << ip;
	rtp->setDestinationIpAddress(ip);
	return PipelineManager::start();
}

int SimpleRtpStreamer::startStreamer()
{
	/* pipelines */
	BaseLmmPipeline *p1 = addPipeline();
	BaseLmmPipeline *p2 = addPipeline();
	/* construct pipeline1 */
	BasePipeElement *pipe1 = p1->appendPipe(camIn);
	pipe1 = p1->appendPipe(enc264High);
	/*
	 * If encoder is not packetized, we need to copy buffers,
	 * otherwise we may optimize a one more memcopy out
	 */
	p1->appendPipe(rtp);
	BasePipeElement::Link fileLink;
	fileLink.destination = p1->addPipe(fout);
	fileLink.destinationChannel = 0;
	fileLink.setRateReduction(0, 0);
	pipe1->addNewLink(fileLink);

	/* construct pipeline2 */
	pipe1 = p2->appendPipe(camIn);
	pipe1->setProcessChannel(-1); /* camera will be processed in 1st pipeline */
	pipe1->setSourceChannel(1);

	/* start pipelines */
	//startElement(p1);
	//startElement(p2);

	return 0;
}

int SimpleRtpStreamer::pipelineOutput(BaseLmmPipeline *pipeline, const RawBuffer &buf)
{
	Q_UNUSED(buf);
	Q_UNUSED(pipeline);
	//ffDebug() << buf.size() << pipeline;
	return 0;
}

QSize SimpleRtpStreamer::getInputSize(int input)
{
	return camIn->getSize(input);
}
