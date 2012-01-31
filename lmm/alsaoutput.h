#ifndef ALSAOUTPUT_H
#define ALSAOUTPUT_H

#include "baselmmoutput.h"
#include <QList>

class Alsa;
class RawBuffer;
class BaseLmmElement;

class AlsaOutput : public BaseLmmOutput
{
	Q_OBJECT
public:
	explicit AlsaOutput(QObject *parent = 0);
	int output();
	int start();
	int stop();
	int flush();
	Alsa * alsaControl() { return alsaOut; }
	qint64 getAvailableBufferTime();
signals:
	
public slots:
private:
	Alsa *alsaOut;
};

#endif // ALSAOUTPUT_H
