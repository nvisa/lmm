#include "x11videooutput.h"

#include <QDebug>

#include <X11/Xlib.h>
#include <X11/extensions/Xvlib.h>

#include <errno.h>
#include <unistd.h>

#define YUV_FOURCC	0x32595559
#define YUV_I420	0x30323449
#define YUV_YV12	0x32315659

class _priv {
public:
	_priv()
	{
		win = 0;
		dpy = NULL;
		adap = -1;
		info = NULL;
	}

	Display *dpy;
	XvAdaptorInfo *info;
	int adap;
	Window win;
	XvImage *im;
	GC gc;
	XGCValues xgcv;
};

X11VideoOutput::X11VideoOutput(QObject *parent) :
	BaseLmmOutput(parent)
{
	p = new _priv;
}

int X11VideoOutput::start()
{
	p->adap = -1;
	uint cnt;
	p->dpy = XOpenDisplay(NULL);
	XvQueryAdaptors(p->dpy, DefaultRootWindow(p->dpy), &cnt, &p->info);
	for (uint i = 0; i < cnt; i++) {
		int fcnt;
		XvImageFormatValues *formats = XvListImageFormats(p->dpy, p->info[i].base_id, &fcnt);
		for (int j = 0; j < fcnt; j++) {
			if (formats[j].format != XvPlanar)
				continue;
			if (formats[j].id == YUV_I420) {
				p->adap = i;
				break;
			}
		}
		XFree(formats);
		if (p->adap != -1)
			break;
	}

	if (p->adap < 0)
		return -EINVAL;

	return BaseLmmOutput::start();
}

int X11VideoOutput::stop()
{
	XFree(p->im);
	XUnmapWindow(p->dpy, p->win);
	XCloseDisplay(p->dpy);
	return BaseLmmOutput::stop();
}

int X11VideoOutput::outputBuffer(RawBuffer buf)
{
	int w = buf.constPars()->videoWidth;
	int h = buf.constPars()->videoHeight;
	if (!p->win) {
		int background = 0x010203;
		p->win = XCreateSimpleWindow(p->dpy, DefaultRootWindow(p->dpy), 0, 0, w, h, 0,
								 WhitePixel(p->dpy, DefaultScreen(p->dpy)), background);
		XMapWindow(p->dpy, p->win);
		p->gc = XCreateGC(p->dpy, p->win, 0, &p->xgcv);
		p->im = XvCreateImage(p->dpy, p->info[p->adap].base_id, YUV_I420, (char *)buf.constData(), w, h);

	}
	p->im->data = (char *)buf.constData();
	XvPutImage(p->dpy, p->info[p->adap].base_id, p->win, p->gc, p->im, 0, 0, w, h, 0, 0, w, h);
	XFlush(p->dpy);
	//usleep(13000);
	return 0;
}
