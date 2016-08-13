#include "genericstreamer.h"

#include <ecl/debug.h>
#include <ecl/settings/applicationsettings.h>

#include <lmm/textoverlay.h>
#include <lmm/dmai/h264encoder.h>
#include <lmm/dm365/dm365camerainput.h>

GenericStreamer::GenericStreamer(QObject *parent) :
	IPCameraStreamer(parent)
{
	ApplicationSettings *s = ApplicationSettings::instance();

	/* camera input settings */
	int vbp = s->get("camera_device.input_vbp").toInt();
	int hbp = s->get("camera_device.input_hbp").toInt();
	int w0 = s->get("camera_device.ch.0.width").toInt();
	int h0 = s->get("camera_device.ch.0.height").toInt();
	int w1 = s->get("camera_device.ch.1.width").toInt();
	int h1 = s->get("camera_device.ch.1.height").toInt();
	int videoFps = s->get("camera_device.output_fps").toInt();
	camIn->setNonStdOffsets(vbp, hbp);
	camIn->setSize(0, QSize(w0, h0));
	camIn->setSize(1, QSize(w1, h1));

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

	/* encoder settings */
	int rateControl = s->get("video_encoding.ch.0.rate_control").toInt();
	int bufCnt = s->get("video_encoding.ch.0.buffer_count").toInt();
	int bitrate = s->get("video_encoding.ch.0.bitrate").toInt();
	int profile = s->get("video_encoding.ch.0.encoding_profile").toInt();
	enc264High->setBitrateControl(DmaiEncoder::RateControl(rateControl));
	enc264High->setBitrate(w0 * h0 * videoFps * 3 * 0.07);
	if (bitrate)
		enc264High->setBitrate(bitrate);
	enc264High->setParameter("videoWidth", w0);
	enc264High->setParameter("videoHeight", h0);
	enc264High->setBufferCount(bufCnt);
	enc264High->setProfile(profile);
	enc264High->setSeiEnabled(s->get("video_encoding.ch.0.sei_enabled").toBool());
	enc264High->setPacketized(s->get("video_encoding.ch.0.packetized").toBool());
	enc264High->setFrameRate(videoFps);
	enc264High->setIntraFrameInterval(s->get("video_encoding.ch.0.intraframe_interval").toInt());
}
