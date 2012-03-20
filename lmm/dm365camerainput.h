#ifndef DM365CAMERAINPUT_H
#define DM365CAMERAINPUT_H

#include "v4l2input.h"

#include <QSemaphore>
#include <QMap>

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
	void aboutDeleteBuffer(RawBuffer *buf);
signals:
	
public slots:
private:
	int openCamera();
	int closeCamera();
	int fpsWorkaround();
	int allocBuffers();
	int configurePreviewer();
	int configureResizer();
	virtual int putFrame(struct v4l2_buffer * buffer);
	virtual v4l2_buffer * getFrame();
	bool captureLoop();

	cameraInput inputType;
	BufTab_Handle bufTab;
	struct _VideoBufDesc *bufDescs;
	int pixFormat;
	int rszFd;
	int preFd;

	QSemaphore bufsFree;
	//QSemaphore bufsTaken;
	QList<Buffer_Handle> finishedDmaiBuffers;
	QMap<Buffer_Handle, RawBuffer *> bufferPool;

	Capture_Handle hCapture;
};

#endif // DM365CAMERAINPUT_H
