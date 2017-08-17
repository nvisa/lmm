#ifndef BASEGSTCAPS_H
#define BASEGSTCAPS_H

#include <QString>

struct _GstCaps;
typedef struct _GstCaps GstCaps;

class BaseGstCaps
{
public:
	BaseGstCaps(GstCaps *caps);
	BaseGstCaps(const QString &mime);

	void setMime(const QString &mime);
	GstCaps * getCaps();
	void setCaps(GstCaps *c, float fps = 0.0);
	bool isEmpty();

	struct video {
		int width;
		int height;
		QString format;
		QString framerate;
	};
	video vid;

protected:
	GstCaps *caps;
};

#endif // BASEGSTCAPS_H
