#ifndef DM365CAMERAINPUT_H
#define DM365CAMERAINPUT_H

#include "v4l2input.h"

#include <QSemaphore>
#include <QMap>
#include <QSize>

#include <xdc/std.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Capture.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/BufferGfx.h>

struct _VideoBufDesc;
struct BufTab_Object;
struct _Buffer_Object;
typedef struct BufTab_Object *BufTab_Handle;
typedef struct _Buffer_Object *Buffer_Handle;

class DM365CameraInput : public V4l2Input
{
	Q_OBJECT
public:
	enum cameraInput {
		COMPOSITE,
		S_VIDEO,
		COMPONENT,
		SENSOR
	};

	explicit DM365CameraInput(QObject *parent = 0);
	void setInputType(cameraInput inp) { inputType = inp; }
	void setInputFps(float fps);
	void setOutputFps(float fps);
	void aboutDeleteBuffer(const QMap<QString, QVariant> &params);
	RawBuffer nextBuffer(int ch);

	int setSize(int ch, QSize sz);
	void setVerticalFlip(int ch, bool flip);
	void setHorizontalFlip(int ch, bool flip);
signals:
	
public slots:
private:
	int openCamera();
	int closeCamera();
	int fpsWorkaround();
	int allocBuffers();
	void clearDmaiBuffers();
	int configurePreviewer();
	int configureResizer();
	virtual int putFrame(struct v4l2_buffer * buffer);
	virtual v4l2_buffer * getFrame();
	bool captureLoop();

	float inputFps;
	float outputFps;
	cameraInput inputType;
	QList<Buffer_Handle> refBuffersA;
	QList<Buffer_Handle> refBuffersB;
	QList<Buffer_Handle> srcBuffers;
	QList<int> useCount;
	int pixFormat;
	int rszFd;
	int preFd;
	int captureWidth2;
	int captureHeight2;
	bool ch1HorFlip;
	bool ch1VerFlip;
	bool ch2HorFlip;
	bool ch2VerFlip;

	QSemaphore bufsFree;
	QList<RawBuffer> outputBuffers2;

	Capture_Handle hCapture;
};

#endif // DM365CAMERAINPUT_H
