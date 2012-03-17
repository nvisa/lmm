#ifndef DM365DMAICAPTURE_H
#define DM365DMAICAPTURE_H

#include "v4l2input.h"

#include <QSize>
#include <QSemaphore>

#include <xdc/std.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Capture.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/BufferGfx.h>

class captureThread;
struct _VideoBufDesc;
struct BufTab_Object;
struct _Buffer_Object;
typedef struct BufTab_Object *BufTab_Handle;
typedef struct _Buffer_Object *Buffer_Handle;

class DM365DmaiCapture : public V4l2Input
{
	Q_OBJECT
public:
	enum cameraInput {
		COMPOSITE,
		S_VIDEO,
		COMPONENT,
		SENSOR
	};
	explicit DM365DmaiCapture(QObject *parent = 0);
	QSize captureSize();
	void setInputType(cameraInput inp) { inputType = inp; }

	void aboutDeleteBuffer(RawBuffer *buf);
signals:
	
public slots:
private:
	int openCamera();
	int closeCamera();
	int putFrameDmai(Buffer_Handle handle);
	Buffer_Handle getFrameDmai();
	bool captureLoop();
	int configurePreviewer();
	int configureResizer();

	cameraInput inputType;
	BufTab_Handle bufTab;
	struct _VideoBufDesc *bufDescs;
	int pixFormat;
	int outPixFormat;
	int rszFd;
	int preFd;

	QSemaphore bufsFree;
	QList<Buffer_Handle> finishedDmaiBuffers;
	QMap<Buffer_Handle, RawBuffer *> bufferPool;

	Capture_Handle hCapture;
};

#endif // DM365DMAICAPTURE_H
