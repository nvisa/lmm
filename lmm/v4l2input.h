#ifndef V4L2INPUT_H
#define V4L2INPUT_H

#include "baselmmelement.h"

#include <QList>
#include <linux/videodev2.h>

class captureThread;
class CircularBuffer;
class QTimer;

class V4l2Input : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit V4l2Input(QObject *parent = 0);

	virtual int start();
	virtual int stop();
	void aboutDeleteBuffer(RawBuffer *buf);

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
	QList<struct v4l2_buffer *> finishedBuffers;
	QList<char *> userptr;
	captureThread *cThread;

	virtual int openCamera();
	virtual int closeCamera();
	virtual int adjustCropping(int width, int height);
	virtual int allocBuffers(unsigned int buf_cnt, enum v4l2_buf_type type);
	virtual int putFrame(struct v4l2_buffer *);
	virtual v4l2_buffer * getFrame();
	virtual bool captureLoop();
};

#endif // V4L2INPUT_H
