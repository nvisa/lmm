#ifndef ALSAINPUT_H
#define ALSAINPUT_H

#include <lmm/baselmmelement.h>

class Alsa;
class AlsaInput : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit AlsaInput(QObject *parent = 0);

	virtual int start();
	virtual int stop();
	virtual int processBlocking(int ch = 0);
	virtual int processBuffer(RawBuffer buf);
signals:
	
public slots:
protected:
	Alsa *alsaIn;
	int bufferSize;
};

#endif // ALSAINPUT_H
