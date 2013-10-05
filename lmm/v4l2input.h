#ifndef V4L2INPUT_H
#define V4L2INPUT_H

#include <lmm/baselmmelement.h>

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
	virtual int flush();
	virtual void aboutDeleteBuffer(const QHash<QString, QVariant> &params);
	virtual int processBlocking(int ch = 0);

	void setBufferCount(int v) { captureBufferCount = v; }
	int getBufferCount() { return captureBufferCount; }
	friend class captureThread;
signals:
	
private slots:

protected:
	unsigned int captureBufferCount;
	int captureWidth;
	int captureHeight;
	QString deviceName;
	int fd;
	int inputIndex;
	QList<struct v4l2_buffer *> v4l2buf;
	QList<char *> userptr;
	QTime timing;

	virtual int openCamera();
	virtual int closeCamera();
	virtual int adjustCropping(int width, int height);
	virtual int allocBuffers(unsigned int buf_cnt, enum v4l2_buf_type type);
	virtual int putFrame(struct v4l2_buffer *);
	virtual v4l2_buffer * getFrame();

	int openDeviceNode();
	int enumStd();
	int enumInput(v4l2_input *input);
	int setInput(v4l2_input *input);
	int setStandard(v4l2_std_id *std_id);
	int queryCapabilities(v4l2_capability *cap);
	int queryStandard();
	int setFormat(unsigned int chromaFormat, int width, int height, bool interlaced = false);
	int startStreaming();
	int stopStreaming();
private:
	int processBuffer(RawBuffer) { return 0; }
	virtual int processBuffer(v4l2_buffer *buffer);
};

#endif // V4L2INPUT_H
