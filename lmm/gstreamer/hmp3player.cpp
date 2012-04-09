#include "hmp3player.h"

#include <emdesk/platform_info.h>
#include <emdesk/platform_info.h>

hMp3Player::hMp3Player(QObject *parent) :
	AbstractGstreamerInterface(parent)
{
}

hMp3Player::hMp3Player(int winId, QObject *parent) :
	AbstractGstreamerInterface(parent)
{
	widgetWinId = winId;
}

void hMp3Player::pipelineCreated()
{
	if (cpu_is_x86()) {
		GstElement *sink = getGstPipeElement("goomSink");
		if (sink != NULL) {
			gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(sink), widgetWinId);
		}
	}
}

int hMp3Player::createElementsList()
{
	GstPipeElement *el = addElement("filesrc", "src");
	el->addParameter("location", mp3FileName);

	if (cpu_is_x86()) {
		el = addElement("audio/mpeg", "");
		el->addParameter("layer", 3);
		el = addElement("ffdec_mp3", "audioDecoder");
	} else {
		el = addElement("mp3parse", "audioParse");
		el = addElement("ffdec_mp3", "audioDecoder");
	}

	el = addElement("alsasink", "audioSink");

	return 0;
}
