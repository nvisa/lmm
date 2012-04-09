#include "h264encoder.h"
#include "dm365camerainput.h"
#include "dm365dmaicapture.h"

#include "fileoutput.h"
#include "dmaiencoder.h"
#include "rawbuffer.h"
#include "fboutput.h"
#include "dm365videooutput.h"
#include "textoverlay.h"
#include "streamtime.h"
#include "rtspserver.h"
#include "debugserver.h"
#include "lmmthread.h"
#include "udpoutput.h"

#include <emdesk/debug.h>

#include <QTimer>
#include <QThread>

#include <errno.h>

#define dmaiCapture() 0

class EncodeThread : public LmmThread
{
public:
	EncodeThread(DmaiEncoder *encoder)
		: LmmThread(encoder->metaObject()->className())
	{
		enc = encoder;
	}
	int operation()
	{
		if (enc->encodeNext())
			QThread::usleep(10000);
		return 0;
	}

private:
	DmaiEncoder *enc;
};

H264Encoder::H264Encoder(QObject *parent) :
	BaseLmmElement(parent)
{
	if (dmaiCapture() == 0)
		input = new DM365CameraInput;
	else
		input = new DM365DmaiCapture;
	elements << input;

	encoder = new DmaiEncoder;
	elements << encoder;

	output =  new FileOutput;
	output->syncOnClock(false);
	//output->setFileName("camera.264", true);
	//output->setFileName("camera.m4e", true);
	output->setFileName("test.264");
	//output->setThreaded(true);
	elements << output;

	DM365VideoOutput *dm365Output = new DM365VideoOutput;
	dm365Output->setVideoOutput(DM365VideoOutput::COMPOSITE);
	output2 = dm365Output;
	output2->syncOnClock(false);
	output2->setThreaded(true);
	elements << output2;

	//rtsp = new RtspServer;
	//elements << rtsp;

	overlay = new TextOverlay(TextOverlay::PIXEL_MAP);
	overlay->setOverlayPosition(QPoint(50, 150));
	overlay->setOverlayText("deneme: no=%1 fps=%2");
	overlay->addOverlayField(TextOverlay::FIELD_FRAME_NO);
	overlay->addOverlayField(TextOverlay::FIELD_STREAM_FPS);
	elements << overlay;

	output3 = new UdpOutput;
	output3->syncOnClock(false);
	output3->setThreaded(false);
	elements << output3;

	timer = new QTimer(this);
	timer->setSingleShot(true);
	connect(timer, SIGNAL(timeout()), this, SLOT(encodeLoop()));

	streamTime = new StreamTime;
	debugServer = new DebugServer;
	debugServer->setElements(&elements);

	useOverlay = false;
	useDisplay = false;
	useFile = false;
	useUdpOutput = true;
	threadedEncode = true;
}

int H264Encoder::start()
{
	foreach (BaseLmmElement *el, elements) {
		mInfo("starting element %s", el->metaObject()->className());
		el->flush();
		int err = el->start();
		if (err)
			return err;
		el->setStreamTime(streamTime);
		connect(el, SIGNAL(needFlushing()), SLOT(flushElements()));
	}
	if (threadedEncode) {
		encoder->setThreaded(true);
		encodeThread = new EncodeThread(encoder);
		encodeThread->start();
	}
	timer->start(10);
	streamTime->start();
	return BaseLmmElement::start();
}

int H264Encoder::stop()
{
	if (threadedEncode) {
		encodeThread->stop();
		encodeThread->wait();
		encodeThread->deleteLater();
	}
	foreach (BaseLmmElement *el, elements) {
		el->setStreamTime(NULL);
		el->stop();
	}
	streamTime->stop();
	return BaseLmmElement::stop();
}

void H264Encoder::encodeLoop()
{
	QTime t; t.start();
	timing.start();
	RawBuffer buf = input->nextBuffer();
	if (buf.size()) {
		debugServer->addCustomStat(DebugServer::STAT_CAPTURE_TIME, timing.restart());
		/* Add overlay if selected */
		if (useOverlay) {
			overlay->addBuffer(buf);
			buf = overlay->nextBuffer();
			debugServer->addCustomStat(DebugServer::STAT_OVERLAY_TIME, timing.restart());
		}
		/* send buffer to encoder */
		encoder->addBuffer(buf);
	}

	if (!threadedEncode)
		encoder->encodeNext();
	RawBuffer buf2 = encoder->nextBuffer();
	if (buf2.size()) {
		debugServer->addCustomStat(DebugServer::STAT_ENCODE_TIME, timing.restart());
		if (useUdpOutput) {
			output3->addBuffer(buf2);
			output3->output();
			debugServer->addCustomStat(DebugServer::STAT_RTSP_OUT_TIME, timing.restart());
		}
		if (useFile) {
			output->addBuffer(buf2);
			output->output();
			debugServer->addCustomStat(DebugServer::STAT_RTSP_OUT_TIME, timing.restart());
			//rtsp->addBuffer(buf2);
		}
	}

	if (buf.size()) {
		if (useDisplay) {
			output2->addBuffer(buf);
			output2->output();
			debugServer->addCustomStat(DebugServer::STAT_DISP_OUT_TIME, timing.restart());
		}
		if (dmaiCapture())
			input->addBuffer(buf);
	}

	if (debugServer->addCustomStat(DebugServer::STAT_LOOP_TIME, t.restart())) {
		//foreach (BaseLmmElement *el, elements)
			//el->printStats();
		mInfo("encoder fps is %d", encoder->getFps());
	}
	timer->start(10);
}

void H264Encoder::flushElements()
{
	mDebug("flushing elements");
	foreach (BaseLmmElement *el, elements) {
		el->flush();
	}
}
