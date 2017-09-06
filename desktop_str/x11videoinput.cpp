#include "x11videoinput.h"

#include <lmm/debug.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <unistd.h>

struct x11vidin {
	Display *display;
	XWindowAttributes gwa;
	Window root;
};

X11VideoInput::X11VideoInput(QObject *parent)
	: BaseLmmElement(parent)
{
	p = new x11vidin;
}

int X11VideoInput::start()
{
	p->display = XOpenDisplay(NULL);
	p->root = DefaultRootWindow(p->display);

	XGetWindowAttributes(p->display, p->root, &p->gwa);
	mDebug("opened display %dx%d", p->gwa.width, p->gwa.height);

	return BaseLmmElement::start();
}

int X11VideoInput::processBlocking(int ch)
{
	//usleep(10 * 1000);
	XImage *img = XGetImage(p->display, p->root, 0, 0, p->gwa.width, p->gwa.height, AllPlanes, ZPixmap);
	RawBuffer buf(this);
	buf.setRefData("image/raw", img->data, img->bytes_per_line * img->height);
	buf.pars()->videoWidth = p->gwa.width;
	buf.pars()->videoHeight = p->gwa.height;
	buf.pars()->avPixelFormat = -1;
	buf.pars()->avFrame = (quintptr *)img;
	return newOutputBuffer(ch, buf);
}

int X11VideoInput::processBuffer(const RawBuffer &buf)
{
	Q_UNUSED(buf);
	return 0;
}

void X11VideoInput::aboutToDeleteBuffer(const RawBufferParameters *pars)
{
	XDestroyImage((XImage *)pars->avFrame);
}

