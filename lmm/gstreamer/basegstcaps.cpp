#include "basegstcaps.h"

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

void BaseGstCaps::setCaps(GstCaps *c)
{
	GstStructure *str = gst_caps_get_structure(c, 0);

	int w, h;
	gst_structure_get_int(str, "width", &w);
	gst_structure_get_int(str, "height", &h);
	const gchar *fmt = gst_structure_get_string(str, "format");

	caps = gst_caps_new_simple("video/x-raw",
					"format", G_TYPE_STRING, fmt,
					"width", G_TYPE_INT, w,
					"height", G_TYPE_INT, h,
					"framerate", GST_TYPE_FRACTION, 25, 2,
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

