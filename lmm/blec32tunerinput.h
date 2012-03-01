#ifndef BLEC32TUNERINPUT_H
#define BLEC32TUNERINPUT_H

#include "v4l2input.h"

struct URLContext;
struct AVInputFormat;

class Blec32TunerInput : public V4l2Input
{
	Q_OBJECT
public:
	explicit Blec32TunerInput(QObject *parent = 0);

	int readPacket(uint8_t *buf, int buf_size);
	int openUrl(QString url, int flags);
	int closeUrl(URLContext *h);
	CircularBuffer * getCircularBuffer() { return circBuf; }

	int start();
	int stop();
signals:
	
public slots:
private:
	CircularBuffer *circBuf;
	AVInputFormat *mpegtsraw;

	int setSystemClock(qint64 time);
	bool captureLoop();
	int openCamera();

	/* mpegts demuxing */
	int vpid;
	int apid;
	int pmt;
	int pcr;
	int sid;
};

#endif // BLEC32TUNERINPUT_H
