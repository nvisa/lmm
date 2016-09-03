#include "genericstreamer.h"
#include "jpegshotserver.h"

#include <ecl/debug.h>
#include <ecl/settings/applicationsettings.h>

#include <lmm/textoverlay.h>
#include <lmm/bufferqueue.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/dmai/seiinserter.h>
#include <lmm/dmai/h264encoder.h>
#include <lmm/dmai/jpegencoder.h>
#include <lmm/rtp/rtptransmitter.h>
#include <lmm/dm365/dm365dmacopy.h>
#include <lmm/rtsp/basertspserver.h>
#include <lmm/dm365/dm365camerainput.h>

float getVideoFps(int pipeline)
{
	ApplicationSettings *s = ApplicationSettings::instance();
	float videoFps = s->get("camera_device.output_fps").toInt();
	if (s->get(QString("config.pipeline.%1.frame_skip.enabled").arg(pipeline)).toBool()) {
		int skip = s->get(QString("config.pipeline.%1.frame_skip.skip").arg(pipeline)).toInt();
		int outOf = s->get(QString("config.pipeline.%1.frame_skip.out_of").arg(pipeline)).toInt();
		videoFps *= (outOf - skip);
		videoFps /= outOf;
	}
	return videoFps;
}

GenericStreamer::GenericStreamer(QObject *parent) :
	BaseStreamer(parent)
{
	ApplicationSettings *s = ApplicationSettings::instance();

	/* camera input settings */
	int vbp = s->get("camera_device.input_vbp").toInt();
	int hbp = s->get("camera_device.input_hbp").toInt();
	int w0 = s->get("camera_device.ch.0.width").toInt();
	int h0 = s->get("camera_device.ch.0.height").toInt();
	int w1 = s->get("camera_device.ch.1.width").toInt();
	int h1 = s->get("camera_device.ch.1.height").toInt();
	camIn = new DM365CameraInput();
	camIn->setNonStdOffsets(vbp, hbp);
	camIn->setSize(0, QSize(w0, h0));
	camIn->setSize(1, QSize(w1, h1));

	/* overlay element, only for High res channel */
	textOverlay = new TextOverlay(TextOverlay::PIXEL_MAP);
	textOverlay->setObjectName("TextOverlay");
	/* overlay settings */
	textOverlay->setEnabled(s->get("text_overlay.enabled").toBool());
	QPoint pt;
	pt.setX(s->get("text_overlay.position.x").toInt());
	pt.setY(s->get("text_overlay.position.y").toInt());
	textOverlay->setOverlayPosition(pt);
	textOverlay->setFontSize(s->get("text_overlay.font_size").toInt());
	textOverlay->setOverlayText(s->get("text_overlay.overlay_text").toString());
	int cnt = s->get("text_overlay.fields.count").toInt();
	for (int i = 0; i < cnt; i++) {
		QString pre = QString("text_overlay.field.%1").arg(i);
		textOverlay->addOverlayField((TextOverlay::overlayTextFields)(s->get(pre + ".type").toInt()),
									 s->get(pre + ".text").toString());
	}

	enc264High = new H264Encoder;
	enc264High->setObjectName("H264EncoderHigh");
	/* first encoder settings */
	int rateControl = s->get("video_encoding.ch.0.rate_control").toInt();
	int bufCnt = s->get("video_encoding.ch.0.buffer_count").toInt();
	int bitrate = s->get("video_encoding.ch.0.bitrate").toInt();
	int profile = s->get("video_encoding.ch.0.encoding_profile").toInt();
	enc264High->setBitrateControl(DmaiEncoder::RateControl(rateControl));
	enc264High->setBitrate(w0 * h0 * getVideoFps(0) * 3 * 0.07);
	if (bitrate)
		enc264High->setBitrate(bitrate);
	enc264High->setParameter("videoWidth", w0);
	enc264High->setParameter("videoHeight", h0);
	enc264High->setBufferCount(bufCnt);
	enc264High->setProfile(profile);
	enc264High->setSeiEnabled(s->get("video_encoding.ch.0.sei_enabled").toBool());
	enc264High->setPacketized(s->get("video_encoding.ch.0.packetized").toBool());
	enc264High->setFrameRate(getVideoFps(0));
	enc264High->setIntraFrameInterval(s->get("video_encoding.ch.0.intraframe_interval").toInt());

	seiInserterHigh = new SeiInserter(enc264High);
	seiInserterHigh->setObjectName("SeiInserterHigh");

	cloner = new DM365DmaCopy(this, 1);
	cloner->setBufferCount(s->get("config.pipeline.0.cloner.buffer_count").toInt());
	cloner->setAllocateSize(s->get("config.pipeline.0.cloner.alloc_size").toInt());

	h264Queue = new BufferQueue(this);
	h264Queue->setQueueSize(s->get("config.pipeline.0.queues.0.queue_size").toInt());
	rawQueue = new BufferQueue(this);
	rawQueue->setQueueSize(s->get("config.pipeline.0.queues.1.queue_size").toInt());

	rtpHigh = new RtpTransmitter(this);
	rtpHigh->setMulticastTTL(s->get("video_encoding.ch.0.onvif.multicast_ttl").toInt());
	rtpHigh->setMaximumPayloadSize(s->get("video_encoding.ch.0.onvif.rtp_max_payload_size").toInt());

	BaseLmmPipeline *p1 = addPipeline();

	p1->append(camIn);
	p1->append(textOverlay);
	p1->append(rawQueue);
	p1->append(enc264High);
	p1->append(seiInserterHigh);
	p1->append(cloner);
	p1->append(h264Queue);
	p1->append(rtpHigh);
	p1->end();

	/* all input and output queues are created at this point so we adjust frame rates here */
	if (s->get("config.pipeline.0.frame_skip.enabled").toBool()) {
		int skip = s->get("config.pipeline.0.frame_skip.skip").toInt();
		int outOf = s->get("config.pipeline.0.frame_skip.out_of").toInt();
		textOverlay->getInputQueue(0)->setRateReduction(skip, outOf);
	}

	/* pipeline 2 */
	enc264Low = new H264Encoder;
	enc264Low->setObjectName("H264EncoderLow");
	/* second encoder settings */
	rateControl = s->get("video_encoding.ch.1.rate_control").toInt();
	bufCnt = s->get("video_encoding.ch.1.buffer_count").toInt();
	bitrate = s->get("video_encoding.ch.1.bitrate").toInt();
	profile = s->get("video_encoding.ch.1.encoding_profile").toInt();
	enc264Low->setBitrateControl(DmaiEncoder::RateControl(rateControl));
	enc264Low->setBitrate(w0 * h0 * getVideoFps(1) * 3 * 0.07);
	if (bitrate)
		enc264Low->setBitrate(bitrate);
	enc264Low->setParameter("videoWidth", w1);
	enc264Low->setParameter("videoHeight", h1);
	enc264Low->setBufferCount(bufCnt);
	enc264Low->setProfile(profile);
	enc264Low->setSeiEnabled(s->get("video_encoding.ch.1.sei_enabled").toBool());
	enc264Low->setPacketized(s->get("video_encoding.ch.1.packetized").toBool());
	enc264Low->setFrameRate(getVideoFps(1));
	enc264Low->setIntraFrameInterval(s->get("video_encoding.ch.1.intraframe_interval").toInt());

	rawQueue2 = new BufferQueue(this);
	rawQueue2->setQueueSize(s->get("config.pipeline.0.queues.2.queue_size").toInt());

	rtpLow = new RtpTransmitter(this);
	rtpLow->setMulticastTTL(s->get("video_encoding.ch.1.onvif.multicast_ttl").toInt());
	rtpLow->setMaximumPayloadSize(s->get("video_encoding.ch.1.onvif.rtp_max_payload_size").toInt());

	BaseLmmPipeline *p2 = addPipeline();
	bool enabled0 = s->get("config.pipeline.0.enabled").toBool();
	bool enabled1 = s->get("config.pipeline.1.enabled").toBool();
	p2->append(camIn);
	if (enabled0 && enabled1) {
		p2->append(rawQueue2);
		p2->append(enc264Low);
		p2->append(rtpLow);
	}
	p2->end();

	if (!enabled0) {
		ElementIOQueue *q1 = camIn->getOutputQueue(0);
		ElementIOQueue *q2 = camIn->getOutputQueue(1);
		camIn->setOutputQueue(0, q2);
		camIn->setOutputQueue(1, q1);

		enc264High->setParameter("videoWidth", w1);
		enc264High->setParameter("videoHeight", h1);
		enc264Low->setParameter("videoWidth", w0);
		enc264Low->setParameter("videoHeight", h0);
	}

	/* all input and output queues are created at this point so we adjust frame rates here */
	if (s->get("config.pipeline.1.frame_skip.enabled").toBool()) {
		int skip = s->get("config.pipeline.1.frame_skip.skip").toInt();
		int outOf = s->get("config.pipeline.1.frame_skip.out_of").toInt();
		rawQueue2->getInputQueue(0)->setRateReduction(skip, outOf);
	}

	/* rtsp server */
	BaseRtspServer *rtsp = new BaseRtspServer(this);
	rtsp->addStream("stream1", false, rtpHigh);
	rtsp->addStream("stream1m", true, rtpHigh, s->get("video_encoding.ch.0.onvif.multicast_port").toInt(),
					s->get("video_encoding.ch.0.onvif.multicast_address").toString());
	rtsp->addStream("stream2", false, rtpLow);
	rtsp->addStream("stream2m", true, rtpLow, s->get("video_encoding.ch.1.onvif.multicast_port").toInt(),
					s->get("video_encoding.ch.1.onvif.multicast_address").toString());

	encJpegHigh = new JpegEncoder;
	encJpegHigh->setQualityFactor(s->get("jpeg_encoding.ch.0.qfact").toInt());
	encJpegHigh->setMaxJpegSize(s->get("jpeg_encoding.ch.0.max_size").toInt());
	encJpegHigh->setParameter("videoWidth", w0);
	encJpegHigh->setParameter("videoHeight", h0);
	encJpegHigh->setStreamTime(p1->getStreamTime());
	encJpegHigh->start();
	encJpegLow = new JpegEncoder;
	encJpegLow->setQualityFactor(s->get("jpeg_encoding.ch.1.qfact").toInt());
	encJpegLow->setMaxJpegSize(s->get("jpeg_encoding.ch.1.max_size").toInt());
	encJpegLow->setParameter("videoWidth", w1);
	encJpegLow->setParameter("videoHeight", h1);
	encJpegLow->setStreamTime(p2->getStreamTime());
	encJpegLow->start();
	if (s->get("jpeg_encoding.jpeg_server.enabled").toBool())
		new JpegShotServer(this, s->get("jpeg_encoding.jpeg_server.port").toInt(), this);
}

QList<RawBuffer> GenericStreamer::getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount)
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
