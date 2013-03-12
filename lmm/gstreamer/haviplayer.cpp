#include "haviplayer.h"

#include <gst/interfaces/xoverlay.h>

#include "debug.h"
#include "platform_info.h"

#include <QFile>

hAviPlayer::hAviPlayer(int winId, QObject *parent) :
	AbstractGstreamerInterface(parent)
{
	widgetWinId = winId;
}

hAviPlayer::hAviPlayer(QObject *parent) :
	AbstractGstreamerInterface(parent)
{
}
#if 0
static void queueOverrunned(GstElement *queue, gpointer data)
{
	qDebug() << "overrun";// << queue->object.name;
}

static void queueUnderrunned(GstElement *queue, gpointer data)
{
	qDebug() << "underrun";// <<queue->object.name;
}

static void queuePushing(GstElement *queue, gpointer data)
{
	qDebug() << queue->object.name << ":";
}
#endif
void hAviPlayer::pipelineCreated()
{
	if (cpu_is_x86()) {
		GstElement *sink = this->getGstPipeElement("videoSink");
		if (sink != NULL) {
			gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(sink), widgetWinId);
		}
	}else {
		//installPadProbes();
		//mDebug("pad probes installed");
#if 0
		GstElement *node = getGstPipeElement("videoQueue");
		if (node) {
			qDebug() << "installing signal handlers for queue element";// << el->nodeName();
			g_signal_connect(node, "overrun", G_CALLBACK(queueOverrunned), NULL);
			g_signal_connect(node, "underrun", G_CALLBACK(queueUnderrunned), NULL);
			g_signal_connect(node, "pushing", G_CALLBACK(queuePushing), NULL);
		}
#endif
	}
}

int hAviPlayer::createVideoElements()
{
	GstPipeElement *el;
	if (cpu_is_x86()) {
		//el = addElement("queue", "videoQueue", "demux");
		el = addElement("ffdec_mpeg4", "videoDecoder");
		el->addSourceLink("demux", "video_00", GstPipeElement::PAD_SOMETIMES, "", GstPipeElement::PAD_ALWAYS);
		/* Add textoverlay if we need subtitles */
		if (subtitleFileName != "") {
			el = addElement("textoverlay","txt", "parser:videoDecoder");
			el->addParameter("valignment",1);
			el->addParameter("ypad",0);
			el->addParameter("shaded-background", false);
			el->addParameter("font-desc","Sans Bold 20");
		}
		el = addElement("xvimagesink", "videoSink");
	} else {
		el = addElement("queue", "videoQueue");
		el->addSourceLink("demux", "video_00", GstPipeElement::PAD_SOMETIMES, "", GstPipeElement::PAD_ALWAYS);
		el = addElement("c6000decoder", "videoDecoder");
		el->addParameter("codec", 2);
		el = addElement("queue", "videoQueue2");
		el = addElement("blec32videosink", "videoSink");
		el->addParameter("scale", 0);
	}

	return 0;
}

int hAviPlayer::createAudioElements()
{
	GstPipeElement *el;
	if (cpu_is_x86()) {
		el = addElement("ffdec_mp3", "audioDecoder");
		el = addElement("alsasink", "audioSink");
	} else {
		if (vFileName.endsWith(".avi")) {
			el = addElement("queue", "audioQueue");
			el->addParameter("max-size-buffers", 0);
			el->addParameter("max-size-time", 0);
			el->addParameter("max-size-bytes", 0);
			el->addSourceLink("demux", "audio_00", GstPipeElement::PAD_SOMETIMES, "", GstPipeElement::PAD_ALWAYS);
			el = addElement("mad", "audioDecoder");
			el = addElement("queue", "audioQueue2");
		} else if (vFileName.endsWith(".mov")) {
			el = addElement("dmaidec_aac", "audioDecoder");
			el->addSourceLink("demux", "audio_00", GstPipeElement::PAD_SOMETIMES, "", GstPipeElement::PAD_ALWAYS);
		}

		el = addElement("blec32audiosink", "audioSink");
		el->addParameter("sync", false);
	}

	return 0;
}

int hAviPlayer::createElementsList()
{
	GstPipeElement *el;
	mInfo("Now watching: %s", qPrintable(vFileName));
	if (subtitleFileName == "" && cpu_is_x86()) {
		QString tmp = vFileName;
		QFile subFile(tmp.replace(".avi", ".srt"));
		if (subFile.exists())
			subtitleFileName = tmp;
	}

	/* Add subtitle parser if user wants */
	if (subtitleFileName != "") {
		el = addElement("filesrc", "subtitleSrc");
		el->addParameter("location", subtitleFileName);
		el = addElement("subparse", "parser");
		el->addParameter("subtitle-encoding","ISO-8859-9");
	}

	/* Video file source */
	el = addElement("filesrc", "videoSrc");
	el->setNoLink(true);
	el->addParameter("location", vFileName);

	/* We need a demux element for AVI files */
	if (cpu_is_x86()) {
		if (vFileName.endsWith(".avi"))
			el = addElement("ffdemux_avi", "demux");
		else if (vFileName.endsWith(".mov"))
			el = addElement("qtdemux", "demux");
	} else {
		if (vFileName.endsWith(".mov"))
			el = addElement("qtdemux", "demux");
		else if (vFileName.endsWith(".avi"))
			el = addElement("ffdemux_avi", "demux");
	}

	createVideoElements();
	//createAudioElements();

	return 0;
}

void hAviPlayer::setSubtitleFileName(QString str)
{
	subtitleFileName = str;
	if (isRunning()) {
		unsigned long long curr = getPlaybackPosition();
		stopPlayback();
		startPlayback();
		sleep(1);
		seekToTime(curr);
	}
}

void hAviPlayer::newApplicationMessageReceived()
{
	GstElement *dec = getGstElement("videoDecoder");
	GstElement *rsz = getGstElement("videoScale");
	GstElement *sink = getGstElement("videoSink");
	if (dec && rsz && sink) {
		GstPad *srcPad = gst_element_get_static_pad(dec, "src");
		if (srcPad) {
			mDebug("blocking pad");
			gst_pad_set_blocked(srcPad, true);
			mDebug("pad blocked");
			gst_element_unlink(dec, rsz);
			mDebug("decoder and resizer unlinked");
			gst_element_unlink(rsz, sink);
			mDebug("resizer and sink unlinked");
			gst_element_link(dec, sink);
			mDebug("decoder and sink linked");
			gst_pad_set_blocked(srcPad, false);
			mDebug("pad un-blocked");
			gst_element_set_state(rsz, GST_STATE_NULL);
			if (gst_bin_remove(GST_BIN(pipeline), rsz))
				mDebug("resizer removed from the pipeline");
		}
	}
}

bool hAviPlayer::newPadBuffer(GstBuffer *buf, GstPipeElement *)
{
	if (cpu_is_x86()) {
		if (ssSync.available())
			return true;
		/*
		 * Create data for the snapshot and add it to list of snapshots
		 * TODO: Free buffers when done
		 */
		char *data = new char[GST_BUFFER_SIZE(buf)];
		memcpy(data, GST_BUFFER_DATA(buf), GST_BUFFER_SIZE(buf));
		snapshots << data;

		/* Get snapshot parameters from buffer caps */
		snapShotSize = GST_BUFFER_SIZE(buf);
		GstStructure *s = gst_caps_get_structure(GST_BUFFER_CAPS(buf), 0);
		gst_structure_get_int(s, "width", &snapShotWidth);
		gst_structure_get_int(s, "height", &snapShotHeight);
		gst_structure_get_fourcc(s, "format", &snapShotFourcc);

		/* Signal other threads */
		ssSync.release(1);
	} else {
		GstCaps *caps = GST_BUFFER_CAPS(buf);
		GstStructure *structure = gst_caps_get_structure(caps, 0);
		int width, height;
		gst_structure_get_int(structure, "width", &width);
		gst_structure_get_int(structure, "height", &height);
		mDebug("new buffer with size %d, width %d height %d", GST_BUFFER_SIZE(buf), width, height);
		if (width & 0x1f) {
			gst_element_post_message (GST_ELEMENT(pipeline),
									  gst_message_new_application(GST_OBJECT(pipeline),
																  gst_structure_new("foo", NULL)));
			return false;
		}
	}

	return true;
}

void hAviPlayer::takeSnapShots(int intervalSecs, int count)
{
	if (cpu_is_arm())
		return;
	for (int i = 0; i < snapshots.size(); ++i)
		delete [] snapshots[i];
	snapshots.clear();
	/* First take current position, so that we can seek back where we left */
	unsigned long long curr = getPlaybackPosition();

	snapShotIntervalSecs = intervalSecs;
	if (snapShotIntervalSecs == 0)
		snapShotIntervalSecs = 60; //defaults to 60 seconds

	seekToTime(curr + snapShotIntervalSecs * 1 * 1000000000ull);
	/* We watch video sink element for data flow */
	installPadProbes();
	/* TODO: Make snapshot count changeable */
	for (int i = 1; i <= count; ++i) {
		ssSync.acquire(1);
		seekToTime(curr + snapShotIntervalSecs * (i + 1)* 1000000000ull);
	}
	/* Remove pads, since we are done with it */
	removePadProbes();

	/* finally seek to where we left */
	seekToTime(curr);
}

