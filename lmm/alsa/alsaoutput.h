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
	int outputBuffer(RawBuffer *buf);
	int start();
	int stop();
	int flush();
	int setParameter(QString param, QVariant value);
	Alsa * alsaControl() { return alsaOut; }
	qint64 getAvailableBufferTime();

	int muteTillFirstOutput();
signals:
	
public slots:
private:
	Alsa *alsaOut;
	bool unmute;
};

#endif // ALSAOUTPUT_H
