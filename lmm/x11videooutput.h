#ifndef X11VIDEOOUTPUT_H
#define X11VIDEOOUTPUT_H

#include <lmm/baselmmoutput.h>

class _priv;

class X11VideoOutput : public BaseLmmOutput
{
	Q_OBJECT
public:
	explicit X11VideoOutput(QObject *parent = 0);
	virtual int start();
	virtual int stop();
protected:
	virtual int outputBuffer(RawBuffer buf);

	_priv *p;
};

#endif // X11VIDEOOUTPUT_H
