#ifndef H264ENCODER_H
#define H264ENCODER_H

#include "baselmmelement.h"

class DmaiEncoder;
class FileOutput;
class DM365CameraInput;
class BaseLmmOutput;
class QTimer;

class H264Encoder : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit H264Encoder(QObject *parent = 0);
	virtual int start();
	virtual int stop();
signals:
public slots:
	void encodeLoop();
private:
	QList<BaseLmmElement *> elements;
	DmaiEncoder *encoder;
	BaseLmmOutput *output;
	DM365CameraInput *input;
	QTimer *timer;
};

#endif // H264ENCODER_H
