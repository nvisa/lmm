#ifndef V4L2OUTPUT_H
#define V4L2OUTPUT_H

#include <lmm/baselmmoutput.h>

#include <QMap>

struct v4l2_buffer;
class RawBuffer;

class V4l2Output : public BaseLmmOutput
{
	Q_OBJECT
public:
	explicit V4l2Output(QObject *parent = 0);
	virtual int outputBuffer(RawBuffer buf);
	virtual int start();
	virtual int stop();
signals:
public slots:
protected:
	QList<v4l2_buffer *> reqBuffers;
	QMap<int, v4l2_buffer *> reqBuffersInUse;
	QMap<int, RawBuffer> buffersInUse;
	int fd;
	QString deviceName;
	int dispWidth;
	int dispHeight;
	bool driverStarted;
	int bufferCount;

	virtual int openDisplay();
	virtual int openDeviceNode();
};

#endif // V4L2OUTPUT_H
