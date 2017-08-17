#include "tk1omxpipeline.h"

#include <lmm/debug.h>

#include <gst/gst.h>

#define Mbit(_x) _x * 1000000

static void setProp(GstElement *el, const QString &name, int value)
{
	GValue val = G_VALUE_INIT;
	g_value_init (&val, G_TYPE_INT);
	g_value_set_int(&val, value);
	g_object_set_property(G_OBJECT(el), qPrintable(name), &val);
	g_value_unset(&val);
}

TK1OmxPipeline::TK1OmxPipeline(QObject *parent)
	: LmmGstPipeline(parent)
{

}

void TK1OmxPipeline::initElements()
{
	LmmGstPipeline::initElements();
#if 0
	GstElement *enc = gst_bin_get_by_name(GST_BIN(bin), "encoder");
	if (enc) {
		qDebug() << "!!!!!!!!!!!!!!!!!!!!!!!!" << GST_IS_PRESET(enc);
		GstPreset *pset = GST_PRESET(enc);
		gchar **list = gst_preset_get_property_names(pset);
		while (*list) {
			qDebug() << *list;
			list++;
		}

		/*setProp(enc, "profile", 2);
		//setProp(enc, "control-rate", 2);
		setProp(enc, "iframeinterval", 13);
		setProp(enc, "bitrate", Mbit(40));

		gint bitrate, control_rate, vbv_size, profile, iframeinterval;
		g_object_get(G_OBJECT(enc), "bitrate", &bitrate, NULL);
		g_object_get(G_OBJECT(enc), "control-rate", &control_rate, NULL);
		g_object_get(G_OBJECT(enc), "vbv-size", &vbv_size, NULL);
		g_object_get(G_OBJECT(enc), "profile", &profile, NULL);
		g_object_get(G_OBJECT(enc), "iframeinterval", &iframeinterval, NULL);

		qDebug() << "!!!!!!!!!!!!!!!!!!!!!" << bitrate << control_rate << vbv_size << profile << iframeinterval;*/
	}
#endif
}

