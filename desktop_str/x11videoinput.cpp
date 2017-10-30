#include "x11videoinput.h"

#include <lmm/debug.h>

#include <QScreen>
#include <QPixmap>
#include <QApplication>
#include <QCoreApplication>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xcomposite.h>

#include <unistd.h>
#include <sys/shm.h>

struct x11vidin {
	Display *display;
	XWindowAttributes gwa;
	Window root;
	//Window overlay;

	XImage *image;
	XShmSegmentInfo shminfo;
	bool useShm;
};

X11VideoInput::X11VideoInput(QObject *parent)
	: BaseLmmElement(parent)
{
	p = new x11vidin;
	p->useShm = false;
}

int X11VideoInput::start()
{
	p->display = XOpenDisplay(NULL);
	p->root = DefaultRootWindow(p->display);
	//p->overlay = XCompositeGetOverlayWindow(p->display, p->root);

	XGetWindowAttributes(p->display, p->root, &p->gwa);
	mDebug("opened display %dx%d", p->gwa.width, p->gwa.height);

	if (p->useShm) {
		p->image = XShmCreateImage(p->display, DefaultVisual(p->display, DefaultScreen(p->display)),
								   p->gwa.depth, ZPixmap, NULL, &p->shminfo, p->gwa.width, p->gwa.height);
		p->shminfo.shmid = shmget(IPC_PRIVATE, p->image->bytes_per_line * p->image->height, IPC_CREAT|0777);
		p->shminfo.shmaddr = p->image->data = (char *)shmat(p->shminfo.shmid, 0, 0);
		p->shminfo.readOnly = False;
		XShmAttach(p->display, &p->shminfo);
	}

	return BaseLmmElement::start();
}

int X11VideoInput::processBlocking(int ch)
{
#ifdef QT_WAY
	const QPixmap &pm = QApplication::primaryScreen()->grabWindow(0);
	const QImage &im = pm.toImage().convertToFormat(QImage::Format_RGB888);
	RawBuffer buf2("image/raw", im.constBits(), im.width() * im.height() * 3);
	buf2.pars()->videoHeight = im.height();
	buf2.pars()->videoWidth = im.width();
	buf2.pars()->avPixelFormat = -1;
	newOutputBuffer(ch, buf2);
	return 0;
#endif
	//usleep(10 * 1000);
	XImage *img = NULL;
	if (p->useShm) {
		XShmGetImage(p->display, p->root, p->image, 0, 0, AllPlanes);
		img = p->image;
	} else {
		img = XGetImage(p->display, p->root, 0, 0, p->gwa.width, p->gwa.height, AllPlanes, ZPixmap);
	}

	//RawBuffer buf("image/raw", img->data, img->bytes_per_line * img->height, this);
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
	if (!p->useShm)
		XDestroyImage((XImage *)pars->avFrame);
}

