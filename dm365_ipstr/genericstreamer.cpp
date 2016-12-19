#include "genericstreamer.h"
#include "mjpegserver.h"
#include "audiosource.h"
#include "metadatagenerator.h"

#include <lmm/alsa/alsainput.h>
#include <lmm/alsa/alsaoutput.h>

#include <ecl/debug.h>
#include <ecl/drivers/hardwareoperations.h>
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
#include <lmm/interfaces/streamcontrolelementinterface.h>

#include <errno.h>

#define getss(nodedef) gets(s, pre, nodedef)

float getVideoFps(int pipeline)
{
	ApplicationSettings *s = ApplicationSettings::instance();
	return s->get(QString("config.pipeline.%1.frame_skip.out_fps").arg(pipeline)).toFloat();
}

static QVariant gets(ApplicationSettings *s, const QString &prefix, const QString &nodedef)
{
	return s->get(QString("%1.%2").arg(prefix).arg(nodedef));
}

GenericStreamer::GenericStreamer(QObject *parent) :
	BaseStreamer(parent)
{
	ApplicationSettings *s = ApplicationSettings::instance();

	/* rtsp server */
	QString rtspConfig = s->get("video_encoding.rtsp.stream_config").toString();
	mDebug("using '%s' RTSP config", qPrintable(rtspConfig));
	BaseRtspServer *rtsp = new BaseRtspServer(this);

	/* camera input settings */
	int cameraInputType = s->get("camera_device.input_type").toInt();
	int vbp = s->get("camera_device.input_vbp").toInt();
	int hbp = s->get("camera_device.input_hbp").toInt();
	int w0 = s->get("camera_device.ch.0.width").toInt();
	int h0 = s->get("camera_device.ch.0.height").toInt();
	int w1 = s->get("camera_device.ch.1.width").toInt();
	int h1 = s->get("camera_device.ch.1.height").toInt();
	int wa[] = {w0, w1};
	int ha[] = {h0, h1};
	camIn = new DM365CameraInput();
	if (cameraInputType) /* we assume no one needs older composite input type for the sake of sane defaults */
		camIn->setInputType((DM365CameraInput::cameraInput)cameraInputType);
	camIn->setNonStdOffsets(vbp, hbp);
	camIn->setSize(0, QSize(w0, h0));
	camIn->setSize(1, QSize(w1, h1));

	QHash<QString, BaseLmmElement *> allElements;
	int err = 0;
	int cnt = s->getArraySize("config.pipeline");
	for (int i = 0; i < cnt; i++) {
		QString p = QString("config.pipeline.%1").arg(i);

		bool fsEnabled = gets(s, p, "frame_skip.enabled").toBool();
		float fsIn = gets(s, p, "frame_skip.in_fps").toFloat();
		float fsOut = gets(s, p, "frame_skip.out_fps").toFloat();
		int fsTarget = gets(s, p, "frame_skip.target_element").toInt();
		int fsQueue = gets(s, p, "frame_skip.target_queue").toInt();

		if (!s->get(QString("%1.enabled").arg(p)).toBool())
			continue;

		mDebug("setting-up pipeline %d", i);

		int streamControlIndex = -1;
		StreamControlElementInterface *controlElement = NULL;
		QList<BaseLmmElement *> elements;
		int ecnt = s->getArraySize(QString("%1.elements").arg(p));
		for (int j = 0; j < ecnt; j++) {
			QString pre = QString("%1.elements.%2").arg(p).arg(j);
			QString type = gets(s, pre, "type").toString();
			QString name = gets(s, pre, "object_name").toString();
			bool enabled = gets(s, pre, "enabled").toBool();
			if (!enabled)
				continue;

			BaseLmmElement *el = NULL;

			if (allElements.contains(name)) {
				el = allElements[name];
			} else if (type == "TextOverlay") {
				el = createTextOverlay(name, s);
			} else if (type == "H264Encoder") {
				int ch = getss("channel").toInt();
				el = createH264Encoder(name, s, ch, wa[ch], ha[ch]);
			} else if (type == "JpegEncoder") {
				int ch = getss("channel").toInt();
				el = createJpegEncoder(name, s, ch, wa[ch], ha[ch]);
				el->start();
			} else if (type == "MjpegElement") {
				MjpegElement *mjpeg = new MjpegElement(getss("port").toInt(), this);
				el = mjpeg;

				if (getss("stream_control.enabled").toBool()) {
					int elCount = s->getArraySize(QString("%1.stream_control.targets").arg(pre));
					for (int k = 0; k < elCount; k++) {
						pre = QString("%1.elements.%2.stream_control.targets.%3").arg(p).arg(j).arg(k);
						streamControlIndex = getss("control_element_index").toInt();
						controlElement = mjpeg;
					}
				}
			} else if (type == "SeiInserter") {
				el = new SeiInserter(this);
			} else if (type == "DM365DmaCopy") {
				DM365DmaCopy *dma = new DM365DmaCopy(this, getss("output_count").toInt());
				dma->setBufferCount(getss("buffer_count").toInt());
				dma->setAllocateSize(getss("alloc_size").toInt());
				el = dma;
			} else if (type == "BufferQueue") {
				BufferQueue *bq = new BufferQueue(this);
				bq->setQueueSize(getss("queue_size").toInt());
				el = bq;
			} else if (type == "RtpTransmitter") {
				QString codec = getss("codec").toString();
				RtpTransmitter *rtp = NULL;
				if (codec.isEmpty() || codec == "h264")
					rtp = new RtpTransmitter(this);
				else if (codec == "g711")
					rtp = new RtpTransmitter(this, Lmm::CODEC_PCM_ALAW);
				else if (codec == "pcm_L16")
					rtp = new RtpTransmitter(this, Lmm::CODEC_PCM_L16);
				else if (codec == "meta_bilkon")
					rtp = new RtpTransmitter(this, Lmm::CODEC_META_BILKON);
				rtp->setMulticastTTL(getss("multicast_ttl").toInt());
				rtp->setMaximumPayloadSize(getss("rtp_max_payload_size").toInt());
				el = rtp;

				int streamCount = s->getArraySize(QString("%1.rtsp").arg(pre));
				for (int k = 0; k < streamCount; k++) {
					pre = QString("%1.elements.%2.rtsp.%3").arg(p).arg(j).arg(k);
					QString streamName = getss("stream").toString();
					if (!rtsp->hasStream(streamName))
						rtsp->addStream(streamName, getss("multicast").toBool(), rtp,
									getss("port").toInt(),
									getss("multicast_address").toString());
					rtsp->addMedia2Stream(getss("media").toString(),
										  streamName,
										  getss("multicast").toBool(),
										  rtp,
										  getss("port").toInt(),
										  getss("multicast_address").toString());

				}
				pre = QString("%1.elements.%2").arg(p).arg(j);

				if (getss("stream_control.enabled").toBool()) {
					int elCount = s->getArraySize(QString("%1.stream_control.targets").arg(pre));
					for (int k = 0; k < elCount; k++) {
						pre = QString("%1.elements.%2.stream_control.targets.%3").arg(p).arg(j).arg(k);
						streamControlIndex = getss("control_element_index").toInt();
						controlElement = rtp;
					}
				}
			} else if (type == "DM365CameraInput") {
				el = camIn;
			} else if (type == "AlsaInput") {
				Lmm::CodecType acodec = Lmm::CODEC_PCM_ALAW;
				QString codecName = getss("codec_name").toString();
				if (codecName == "g711")
					acodec = Lmm::CODEC_PCM_ALAW;
				else if (codecName == "L16")
					acodec = Lmm::CODEC_PCM_L16;
				AlsaInput *alsaIn = new AlsaInput(acodec, this);
				alsaIn->setParameter("audioRate", getss("rate").toInt());
				alsaIn->setParameter("sampleCount", getss("buffer_size").toInt());
				alsaIn->setParameter("channelCount", getss("channels").toInt());
				el = alsaIn;
			} else if (type == "AlsaOutput") {
				AlsaOutput *alsaOut = new AlsaOutput(this);
				alsaOut->setParameter("audioRate", getss("rate").toInt());
				alsaOut->setParameter("channelCount", getss("channels").toInt());
				el = alsaOut;
			} else if (type == "AudioSource") {
				AudioSource *src = new AudioSource(this);
				el = src;
			} else if (type == "MetadataGenerator") {
				MetadataGenerator *mgen = new MetadataGenerator(this);
				el = mgen;
			}

			if (!el) {
				mDebug("no suitable element for node %d (%s)", j, qPrintable(type));
				err = -ENOENT;
				break;
			}
			if (!name.isEmpty())
				el->setObjectName(name);
			mDebug("adding element %s with type %s to pipeline %d", qPrintable(name), qPrintable(type), i);
			elements << el;
			allElements.insert(name, el);
		}

		BaseLmmPipeline *pl = addPipeline();
		for (int j = 0; j < elements.size(); j++)
			pl->append(elements[j]);
		pl->end();
		streamControl.insert(pl, streamControlIndex);
		streamControlElement.insert(pl, controlElement);

		if (fsEnabled)
			pl->getPipe(fsTarget)->getOutputQueue(fsQueue)->setRateReduction(fsIn, fsOut);
	}

	if (s->get("camera_device.invert_clock").toBool())
		HardwareOperations::writeRegister(0x1c40044, 0x1c);
	else
		HardwareOperations::writeRegister(0x1c40044, 0x18);
}

QList<RawBuffer> GenericStreamer::getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount)
{
	return QList<RawBuffer>();
#if 0
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
#endif
}

int GenericStreamer::pipelineOutput(BaseLmmPipeline *p, const RawBuffer &)
{
	int sci = streamControl[p];
	if (sci != -1) {
		StreamControlElementInterface *sce = streamControlElement[p];
		BaseLmmElement *el = p->getPipe(sci);
		bool active = sce->isActive();
		if (active && el->isPassThru()) {
			mDebug("enabling pipeline control point: %d", sci);
			for (int i = sci; i < p->getPipeCount(); i++)
				p->getPipe(i)->setPassThru(false);
		} else if (!active && !el->isPassThru()) {
			mDebug("disabling pipeline control point: %d", sci);
			for (int i = p->getPipeCount() - 1; i >= sci; i--)
				p->getPipe(i)->setPassThru(true);
		}
	}
	return 0;
}

TextOverlay * GenericStreamer::createTextOverlay(const QString &elementName, ApplicationSettings *s)
{
	/* overlay element, only for High res channel */
	TextOverlay *textOverlay = new TextOverlay(TextOverlay::PIXEL_MAP);
	textOverlay->setObjectName(elementName);
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
	return textOverlay;
}

H264Encoder * GenericStreamer::createH264Encoder(const QString &elementName, ApplicationSettings *s, int ch, int w0, int h0)
{
	H264Encoder *enc = new H264Encoder;
	enc->setObjectName(elementName);
	/* first encoder settings */
	int rateControl = s->get(QString("video_encoding.ch.%1.rate_control").arg(ch)).toInt();
	int bufCnt = s->get(QString("video_encoding.ch.%1.buffer_count").arg(ch)).toInt();
	int bitrate = s->get(QString("video_encoding.ch.%1.bitrate").arg(ch)).toInt();
	int profile = s->get(QString("video_encoding.ch.%1.encoding_profile").arg(ch)).toInt();
	enc->setBitrateControl(DmaiEncoder::RateControl(rateControl));
	enc->setBitrate(w0 * h0 * getVideoFps(ch) * 3 * 0.07);
	if (bitrate)
		enc->setBitrate(bitrate);
	enc->setParameter("videoWidth", w0);
	enc->setParameter("videoHeight", h0);
	enc->setBufferCount(bufCnt);
	enc->setProfile(profile);
	enc->setSeiEnabled(s->get(QString("video_encoding.ch.%1.sei_enabled").arg(ch)).toBool());
	enc->setPacketized(s->get(QString("video_encoding.ch.%1.packetized").arg(ch)).toBool());
	enc->setFrameRate(getVideoFps(ch));
	enc->setIntraFrameInterval(s->get(QString("video_encoding.ch.%1.intraframe_interval").arg(ch)).toInt());
	enc->setMotionVectorExtraction(H264Encoder::MotionVectors(s->get(QString("video_encoding.ch.%1.motion_detection").arg(ch)).toInt()));
	enc->setMotionDetectionThreshold(s->get(QString("video_encoding.ch.%1.motion_detection_threshold").arg(ch)).toInt());
	return enc;
}

JpegEncoder * GenericStreamer::createJpegEncoder(const QString &elementName, ApplicationSettings *s, int ch, int w0, int h0)
{
	JpegEncoder *enc = new JpegEncoder;
	enc->setObjectName(elementName);
	enc->setQualityFactor(s->get(QString("jpeg_encoding.ch.%1.qfact").arg(ch)).toInt());
	enc->setMaxJpegSize(s->get(QString("jpeg_encoding.ch.%1.max_size").arg(ch)).toInt());
	enc->setParameter("videoWidth", w0);
	enc->setParameter("videoHeight", h0);
	return enc;
}
