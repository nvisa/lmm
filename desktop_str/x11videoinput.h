#ifndef X11VIDEOINPUT_H
#define X11VIDEOINPUT_H

#include <lmm/baselmmelement.h>

struct x11vidin;

class X11VideoInput : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit X11VideoInput(QObject *parent = 0);

	int start();
signals:

public slots:
protected:
	int processBlocking(int ch);
	int processBuffer(const RawBuffer &buf);
	void aboutToDeleteBuffer(const RawBufferParameters *pars);

	x11vidin *p;
};

#endif // X11VIDEOINPUT_H
