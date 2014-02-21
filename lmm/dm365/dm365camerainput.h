#ifndef DM365CAMERAINPUT_H
#define DM365CAMERAINPUT_H

#include <lmm/v4l2input.h>

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
	cameraInput getInputType() { return inputType; }
	void setInputType(cameraInput inp) { inputType = inp; }
	void setInputFps(float fps);
	void setOutputFps(float fps);
	int setInputSize(QSize sz);
	int setOutputSize(int ch, QSize sz);
	void setNonStdOffsets(int vbp, int hbp);
	void aboutToDeleteBuffer(const RawBufferParameters *params);

	int setSize(int ch, QSize sz);
	QSize getSize(int ch);
	void setVerticalFlip(int ch, bool flip);
	void setHorizontalFlip(int ch, bool flip);

	virtual QList<QVariant> extraDebugInfo();
signals:
	
public slots:
protected:
	void calculateFps(const RawBuffer buf);
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
	int processBuffer(v4l2_buffer *buffer);

	float inputFps;
	float outputFps;
	cameraInput inputType;
	QList<Buffer_Handle> refBuffersA;
	QList<Buffer_Handle> refBuffersB;
	QList<Buffer_Handle> srcBuffers;
	QList<int> useCount;
	QMutex useLock;
	int pixFormat;
	int rszFd;
	int preFd;
	int inputCaptureWidth;
	int inputCaptureHeight;
	int captureWidth2;
	int captureHeight2;
	bool ch1HorFlip;
	bool ch1VerFlip;
	bool ch2HorFlip;
	bool ch2VerFlip;

	QSemaphore bufsFree;
	Capture_Handle hCapture;

	/* non-std mode items */
	struct ISIFMode {
		uint hdw;
		uint vdw;
		uint ppln;
		uint lpfr;
		uint sph;
		uint lnh;
		uint slv0;
		uint slv1;
		uint culh;
		uint culv;
		uint hsize;
		uint sdofst;
		uint cadu;
		uint cadl;
	};
	struct IPIPEMode {
		uint vps;
		uint vsz;
		uint hps;
		uint hsz;
	};
	struct UseNonStdInput {
		int vbp;
		int hbp;
		bool enabled;
	};
	UseNonStdInput nonStdInput;

	int setupISIF(ISIFMode mode);
	int setupIPIPE(IPIPEMode mode);
	int setupNonStdMode();
};

#endif // DM365CAMERAINPUT_H
