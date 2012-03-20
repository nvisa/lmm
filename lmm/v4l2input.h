#ifndef V4L2INPUT_H
#define V4L2INPUT_H

#include "baselmmelement.h"

#include <QList>
#include <QTime>
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
	virtual void aboutDeleteBuffer(RawBuffer *buf);

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
	QTime timing;

	virtual int openCamera();
	virtual int closeCamera();
	virtual int adjustCropping(int width, int height);
	virtual int allocBuffers(unsigned int buf_cnt, enum v4l2_buf_type type);
	virtual int putFrame(struct v4l2_buffer *);
	virtual v4l2_buffer * getFrame();
	virtual bool captureLoop();

	int openDeviceNode();
	int enumInput(v4l2_input *input);
	int setInput(v4l2_input *input);
	int setStandard(v4l2_std_id *std_id);
	int queryCapabilities(v4l2_capability *cap);
	int queryStandard();
	int setFormat(unsigned int chromaFormat, int width, int height);
	int startStreaming();
	int stopStreaming();
};

#endif // V4L2INPUT_H
