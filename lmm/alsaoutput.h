#ifndef ALSAOUTPUT_H
#define ALSAOUTPUT_H

#include <QObject>
#include <QList>
class Alsa;
class RawBuffer;

class AlsaOutput : public QObject
{
	Q_OBJECT
public:
	explicit AlsaOutput(QObject *parent = 0);
	int addBuffer(RawBuffer *buffer);
	int output();
signals:
	
public slots:
private:
	Alsa *alsaOut;
	QList<RawBuffer *> buffers;
};

#endif // ALSAOUTPUT_H
