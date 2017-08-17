#include "basegstcaps.h"
#include "debug.h"

#include <gst/gst.h>

BaseGstCaps::BaseGstCaps(GstCaps *caps)
{
	caps = caps;
}

BaseGstCaps::BaseGstCaps(const QString &mime)
{
	setMime(mime);
}

void BaseGstCaps::setMime(const QString &mime)
{
	if (mime.isEmpty())
		caps = NULL;
	else
		caps = gst_caps_from_string(qPrintable(mime));
}

GstCaps *BaseGstCaps::getCaps()
{
	return caps;
}

void BaseGstCaps::setCaps(GstCaps *c, float fps)
{
	GstStructure *str = gst_caps_get_structure(c, 0);

	int num = 0;
	int den = 1;
	int w, h;
	gst_structure_get_int(str, "width", &w);
	gst_structure_get_int(str, "height", &h);
	const gchar *fmt = gst_structure_get_string(str, "format");
	const GValue *frate = gst_structure_get_value(str, "framerate");
	if (GST_VALUE_HOLDS_FRACTION(frate)) {
		gint gnum = gst_value_get_fraction_numerator(frate);
		gint gden = gst_value_get_fraction_denominator(frate);
		if (num) {
			num = gnum;
			den = gden;
		} else if (fps > 0) {
			int fpsn = fps;
			if (qAbs(fps - fpsn) > 0.001) {
				num = fps * 2;
				den = 2;
			} else {
				num = fps;
				den = 1;
			}
		}
	}

	caps = gst_caps_new_simple("video/x-raw",
					"format", G_TYPE_STRING, fmt,
					"width", G_TYPE_INT, w,
					"height", G_TYPE_INT, h,
					"framerate", GST_TYPE_FRACTION, num, den,
					NULL);

	gchar *framerate = gst_value_serialize(gst_structure_get_value(gst_caps_get_structure(caps, 0),"framerate"));
	vid.width = w;
	vid.height = h;
	vid.format = QString::fromLatin1(fmt);
	vid.framerate = QString::fromLatin1(framerate);
	//g_free(&fmt);
}

bool BaseGstCaps::isEmpty()
{
	return caps ? false : true;
}

