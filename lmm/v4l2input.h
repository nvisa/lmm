#ifndef V4L2INPUT_H
#define V4L2INPUT_H

#include <lmm/baselmmelement.h>

#include <QList>
#include <QElapsedTimer>

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
	virtual int prepareStop();
	virtual int flush();
	virtual void aboutToDeleteBuffer(const RawBufferParameters *params);
	virtual int processBlocking(int ch = 0);

	int info();
	int setFrameSkip(int cnt);

	void setBufferCount(int v) { captureBufferCount = v; }
	int getBufferCount() { return captureBufferCount; }
	friend class captureThread;

	virtual int startStreaming();
	virtual int stopStreaming();
	void setManualStart(bool v) { manualStart = v; }

	void listControls(const QString &filename);
	int getControlValue(const QString &filename, int controlId);
	int setControlValue(const QString &filename, int controlId, int val);
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
	QElapsedTimer timing;
	bool nonBlockingIO;
	bool manualStart;
	int frameSkipCount;
	int frameSkip;
	int captureCount;
	int pixelFormat;

	virtual int openCamera();
	virtual int closeCamera();
	virtual int adjustCropping(int width, int height, int left = 0, int top = 0);
	virtual int allocBuffers(unsigned int buf_cnt, enum v4l2_buf_type type);
	virtual int putFrame(struct v4l2_buffer *);
	virtual v4l2_buffer * getFrame();

	int openDeviceNode();
	int enumStd();
	int enumFmt(int index);
	int enumInput(v4l2_input *input);
	int setInput(v4l2_input *input);
	int setStandard(v4l2_std_id *std_id);
	int queryCapabilities(v4l2_capability *cap);
	int queryStandard();
	int setFormat(unsigned int chromaFormat, int width, int height, bool interlaced = false);
	virtual int processBuffer(v4l2_buffer *buffer);
private:
	int processBuffer(const RawBuffer &) { return 0; }
};

#endif // V4L2INPUT_H
