#include "camerastreamer.h"
#include "dm365camerainput.h"
#include "dm365dmaicapture.h"
#include "fileoutput.h"
#include "h264encoder.h"
#include "jpegencoder.h"
#include "rawbuffer.h"
#include "fboutput.h"
#include "dm365videooutput.h"
#include "textoverlay.h"
#include "streamtime.h"
#include "debugserver.h"
#include "lmmthread.h"
#include "udpoutput.h"
#include "cpuload.h"
#include "rtspoutput.h"
#include "videotestsource.h"
#include "mp4mux.h"
#include "avimux.h"
#include "rtspserver.h"
#include "vlc/vlcrtspstreamer.h"

#include <emdesk/debug.h>

#include <QTimer>
#include <QThread>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>

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

/**
	\class CameraStreamer

	\brief Bu sinif DM365 uzerinde kamera goruntusunu alip ag uzerinden
	farkli bicimlerde akis olarak gonderebilen bir siniftir.

	Not: Bu sinif VK'ya ozel bir hal aldi. Yeri yeniden konumlandirilmali.

	\ingroup camera

	This class implements a multimedia interface for the application.
	Bu sinif VK'nin dis dunya ile arayuzunu saglar. Kamera modullerinden
	alinan goruntunun farkli ag akislari uzerinden gonderilmesini saglar.

	Varsayilan calisma modu:

		- RTSP akis
			* RTSP akisi liveMedia555 uzerinden fifo kullanilarak

		- Text overlay kapali
		- Yerel video output yok (LCD)
		- Dosyaya kayit yok

	\sa VideoFile, AudioFile
*/

CameraStreamer::CameraStreamer(QObject *parent) :
	BaseLmmElement(parent)
{
	/* ensure CpuLoad is started in correct thread context */
	CpuLoad::getCpuLoad();
	if (dmaiCapture() == 0)
		input = new DM365CameraInput;
	else
		input = new DM365DmaiCapture;
	elements << input;

	testInput = new VideoTestSource;
	elements << testInput;

	jpegEncoder = new JpegEncoder;
	elements << jpegEncoder;
	jpegShotInterval = 1000;

	h264Encoder = new H264Encoder;
	elements << h264Encoder;

	output =  new FileOutput;
	output->syncOnClock(false);
	output->setFileName("camera.264");
	//elements << output;

	jpegFileOutput = new FileOutput;
	jpegFileOutput->syncOnClock(false);
	jpegFileOutput->setThreaded(true);
	jpegFileOutput->setIncremental(true);
	jpegFileOutput->setFileName("sshot.jpg");
	elements << jpegFileOutput;

	videoOutputType = Lmm::COMPOSITE;
	dm365Output = new DM365VideoOutput;
	dm365Output->setVideoOutput(videoOutputType);
	dm365Output->syncOnClock(false);
	dm365Output->setThreaded(true);
	//elements << dm365Output;

	/*rtspServer = new RtspServer;
	elements << rtspServer;*/

	overlay = new TextOverlay(TextOverlay::PIXEL_MAP);
	//elements << overlay;

	/*mux = new Mp4Mux;
	elements << mux;*/

	/*rtspVlc = new VlcRtspStreamer;
	elements << rtspVlc;*/

	streamingType = RTSP;

	output3 = new UdpOutput;
	output3->syncOnClock(false);
	output3->setThreaded(false);
	//elements << output3;

	rtspOutput = new RtspOutput;
	rtspOutput->syncOnClock(false);
	rtspOutput->setThreaded(false);
	elements << rtspOutput;
	connect(rtspOutput, SIGNAL(newSessionCreated(RtspOutput::sessionType)),
			SLOT(newRtspSessionCreated(RtspOutput::sessionType)));

	timer = new QTimer(this);
	timer->setSingleShot(true);
	connect(timer, SIGNAL(timeout()), this, SLOT(encodeLoop()));

	streamTime = new StreamTime;
	debugServer = new DebugServer;
	debugServer->setElements(&elements);

	muxType = 0;
	useTestInput = false;
	useOverlay = false;
	useDisplay = false;
	useFile = false;
	useStreamOutput = true;
	threadedEncode = true;
	useFileIOForRtsp = false;
	flushForSpsPps = false;

	imageWidth = 1280;
	imageHeight = 720;
}

int CameraStreamer::start()
{
	if (useTestInput) {
		testInput->setParameter("videoWidth", imageWidth);
		testInput->setParameter("videoHeight", imageHeight);
		((VideoTestSource *)testInput)->setTestPattern(VideoTestSource::COLORCHART);
	}
	foreach (BaseLmmElement *el, elements) {
		mInfo("starting element %s", el->metaObject()->className());
		el->flush();
		el->setStreamTime(streamTime);
		/*
		 * set element parameters, not all parameters are used
		 * in all elements but we notify all of them. These can be
		 * regarded as global pipeline parameters
		 */
		el->setParameter("videoWidth", imageWidth);
		el->setParameter("videoHeight", imageHeight);

		/* start element */
		int err = el->start();
		if (err) {
			mDebug("error starting element %s", el->metaObject()->className());
			return err;
		}
		connect(el, SIGNAL(needFlushing()), SLOT(flushElements()));
	}
	if (threadedEncode) {
		h264Encoder->setThreaded(true);
		encodeThread = new EncodeThread(h264Encoder);
		encodeThread->start();
		encoder = h264Encoder;
		jpegEncoder->setThreaded(true);
		jpegEncodeThread = new EncodeThread(jpegEncoder);
		jpegEncodeThread->start();
	}
	timer->start(10);
	streamTime->start();
	jpegTime.start();
	return BaseLmmElement::start();
}

int CameraStreamer::stop()
{
	if (threadedEncode) {
		encodeThread->stop();
		encodeThread->wait();
		encodeThread->deleteLater();
		jpegEncodeThread->stop();
		jpegEncodeThread->wait();
		jpegEncodeThread->deleteLater();
	}
	foreach (BaseLmmElement *el, elements) {
		mInfo("stopping element %s", el->metaObject()->className());
		el->setStreamTime(NULL);
		el->stop();
	}
	streamTime->stop();
	return BaseLmmElement::stop();
}

void CameraStreamer::addTextOverlay(QString text)
{
	if (text.isEmpty())
		useOverlay = false;
	else
		useOverlay = true;
	overlay->setOverlayText(text);
}

void CameraStreamer::addTextOverlayParameter(TextOverlay::overlayTextFields field
											 , QString val)
{
	overlay->addOverlayField(field, val);
}

void CameraStreamer::setTextOverlayPosition(QPoint pos)
{
	overlay->setOverlayPosition(pos);
}

void CameraStreamer::useDisplayOutput(bool v, Lmm::VideoOutput type)
{
	if (v) {
		if (type != videoOutputType) {
			videoOutputType = type;
			dm365Output->setVideoOutput(type);
		}
	}
	useDisplay = v;
}

void CameraStreamer::useFileOutput(QString fileName)
{
	if (fileName.isEmpty())
		useFile = false;
	else
		useFile = true;
	output->setFileName(fileName);
}

void CameraStreamer::useStreamingOutput(bool v,
										CameraStreamer::StreamingProtocol proto)
{
	if (v == true) {
		streamingType = proto;
		useStreamOutput = true;
	} else {
		useStreamOutput = false;
	}
}

void CameraStreamer::encodeLoop()
{
	QTime t; t.start();
	timing.start();
	RawBuffer buf;
	if (useTestInput)
		buf = testInput->nextBuffer();
	else
		buf = input->nextBuffer();
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

	if (flushForSpsPps) {
		h264Encoder->flush();
		flushForSpsPps = 0;
	}
	if (!threadedEncode)
		encoder->encodeNext();
	RawBuffer buf2 = encoder->nextBuffer();
	if (muxType) {
		/* mux first */
		if (buf2.size())
			mux->addBuffer(buf2);
		buf2 = mux->nextBuffer();
	}
	if (buf2.size()) {
		mInfo("sending encoded buffer");
		debugServer->addCustomStat(DebugServer::STAT_ENCODE_TIME, timing.restart());
		if (useStreamOutput) {
			if (streamingType == RTSP) {
				if (!useFileIOForRtsp) { //due to live555 rtsp bug
					rtspOutput->addBuffer(buf2);
					rtspOutput->output();
					h264Encoder->setSeiLoopLatency(rtspOutput->getLoopLatency());
				} else {
					output->addBuffer(buf2);
					output->output();
				}
			} else {
				output3->addBuffer(buf2);
				output3->output();
			}
			debugServer->addCustomStat(DebugServer::STAT_RTSP_OUT_TIME, timing.restart());
		}
		if (useFile) {
			output->addBuffer(buf2);
			output->output();
			debugServer->addCustomStat(DebugServer::STAT_RTSP_OUT_TIME, timing.restart());
		}
	}

	if (buf.size()) {
		if (useDisplay) {
			dm365Output->addBuffer(buf);
			dm365Output->output();
			debugServer->addCustomStat(DebugServer::STAT_DISP_OUT_TIME, timing.restart());
		}
		if (dmaiCapture())
			input->addBuffer(buf);
		if (useTestInput)
			testInput->addBuffer(buf);
		/*
		 * If we are in jpeg streaming mode we do not take
		 * snapshots so we don't need to feed extra buffers
		 */
		if (getSessionCodec(sessionType) != Lmm::CODEC_JPEG) {
			if (jpegTime.elapsed() > jpegShotInterval) {
				jpegEncoder->addBuffer(buf);
				if (!threadedEncode)
					jpegEncoder->encodeNext();
				jpegTime.start();
			}
		}
	}
	/* get snapshot if we are not jpeg streaming */
	if (getSessionCodec(sessionType) != Lmm::CODEC_JPEG) {
		buf2 = jpegEncoder->nextBuffer();
		if (buf2.size())
			jpegFileOutput->addBuffer(buf2);
	}
	if (debugServer->addCustomStat(DebugServer::STAT_LOOP_TIME, t.restart())) {
		mInfo("encoder fps is %d", encoder->getFps());
	}

	timer->start(10);
}

void CameraStreamer::flushElements()
{
	mDebug("flushing elements");
	foreach (BaseLmmElement *el, elements) {
		el->flush();
	}
}

void CameraStreamer::newRtspSessionCreated(RtspOutput::sessionType type)
{
	Lmm::CodecType ctype = getSessionCodec(type);
	if (ctype == Lmm::CODEC_H264) {
		encoder = h264Encoder;
		flushForSpsPps = 1;
	} else if (ctype == Lmm::CODEC_JPEG) {
		encoder = jpegEncoder;
	}
	sessionType = type;
}

Lmm::CodecType CameraStreamer::getSessionCodec(RtspOutput::sessionType type)
{
	if (type == RtspOutput::H264_UNICAST
			|| type == RtspOutput::H264_MULTICAST
			|| type == RtspOutput::H264_LOWRES_UNICAST
			|| type == RtspOutput::H264_LOWRES_MULTICAST
			)
		return Lmm::CODEC_H264;
	if (type == RtspOutput::MJPEG_MULTICAST
			|| type == RtspOutput::MJPEG_UNICAST)
		return Lmm::CODEC_JPEG;
	return Lmm::CODEC_MPEG4;
}

void CameraStreamer::useThreadedEncode(bool v)
{
	if (v == threadedEncode)
		return;
	if (v == true) {
		h264Encoder->setThreaded(true);
		encodeThread = new EncodeThread(h264Encoder);
		encodeThread->start();
		jpegEncodeThread = new EncodeThread(jpegEncoder);
		jpegEncodeThread->start();
		jpegEncoder->setThreaded(true);
	} else {
		encodeThread->stop();
		encodeThread->wait();
		encodeThread->deleteLater();
		jpegEncodeThread->stop();
		jpegEncodeThread->wait();
		jpegEncodeThread->deleteLater();
	}
	threadedEncode = v;
}

void CameraStreamer::readImplementationSettings(QXmlStreamReader *xml,
												QString sectionName)
{
	QString name, str;
	while (!xml->atEnd()) {
		xml->readNext();
		if (xml->isEndElement()) {
			name = xml->name().toString();
			if (name == sectionName)
				break;
		}
		if (xml->isCharacters()) {
			str = xml->text().toString().trimmed();
			if (!str.isEmpty()) {
				if (name == "threaded_encode")
					threadedEncode = str.toInt();
				else if (name == "threaded_display")
					dm365Output->setThreaded(str.toInt());
				else if (name == "threaded_prop_streaming")
					output3->setThreaded(str.toInt());
				else if (name == "threaded_rtsp_streaming")
					rtspOutput->setThreaded(str.toInt());
				else if (name == "threaded_file_output")
					output->setThreaded(str.toInt());
				else if (name == "use_file_for_rtsp")
					useFileIOForRtsp = str.toInt();
				else if (name == "capture_buffer_count")
					input->setBufferCount(str.toInt());
			}
		}
		if (xml->isStartElement()) {
			name = xml->name().toString();
		}
	}
}

void CameraStreamer::writeImplementationSettings(QXmlStreamWriter *wr)
{
	wr->writeStartElement("implementation_details");
	wr->writeTextElement("threaded_encode", QString::number(threadedEncode));
	wr->writeTextElement("threaded_display",  QString::number(dm365Output->isThreaded()));
	wr->writeTextElement("threaded_prop_streaming",  QString::number(output3->isThreaded()));
	wr->writeTextElement("threaded_rtsp_streaming",  QString::number(rtspOutput->isThreaded()));
	wr->writeTextElement("threaded_file_output",  QString::number(output->isThreaded()));
	wr->writeTextElement("use_file_for_rtsp",  QString::number(useFileIOForRtsp));
	wr->writeTextElement("capture_buffer_count", QString::number(input->getBufferCount()));
	wr->writeEndElement();
}
