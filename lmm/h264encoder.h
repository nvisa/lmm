#ifndef H264ENCODER_H
#define H264ENCODER_H

#include "baselmmelement.h"

#include <QTime>

class DmaiEncoder;
class FileOutput;
class V4l2Input;
class BaseLmmOutput;
class QTimer;
class TextOverlay;
class RtspServer;
class DebugServer;
class EncodeThread;

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
	void flushElements();
private:
	QList<BaseLmmElement *> elements;
	DmaiEncoder *encoder;
	FileOutput *output;
	BaseLmmOutput *output2;
	BaseLmmOutput *output3;
	V4l2Input *input;
	TextOverlay *overlay;
	QTimer *timer;
	RtspServer *rtsp;
	QTime timing;
	DebugServer *debugServer;
	EncodeThread *encodeThread;

	bool useOverlay;
	bool useDisplay;
	bool useFile;
	bool useUdpOutput;
	bool threadedEncode;

	enum Operation {
		CAPTURE,
		OVERLAY,
	};

	int operation(RawBuffer buf, Operation oper);
};

#endif // H264ENCODER_H
