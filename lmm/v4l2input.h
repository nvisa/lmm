#ifndef V4L2INPUT_H
#define V4L2INPUT_H

#include "baselmmelement.h"

#include <QList>

extern "C" {
	#include <linux/videodev2.h>
}

class captureThread;
class CircularBuffer;
class QTimer;

class V4l2Input : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit V4l2Input(QObject *parent = 0);

	CircularBuffer * getCircularBuffer() { return circBuf; }
	int start();
	int stop();

	friend class captureThread;
signals:
	
private slots:

protected:
	int captureWidth;
	int captureHeight;
	QString deviceName;
	int fd;
	int inputIndex;
	QList<struct v4l2_buffer *> v4l2buf;
	QList<char *> userptr;
	CircularBuffer *circBuf;
	captureThread *cThread;

	virtual int openCamera();
	virtual int closeCamera();
	int adjustCropping(int width, int height);
	int allocBuffers(unsigned int buf_cnt, enum v4l2_buf_type type);
	virtual int putFrame(struct v4l2_buffer *);
	virtual v4l2_buffer * getFrame();
};

#endif // V4L2INPUT_H
