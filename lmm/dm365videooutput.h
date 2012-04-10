#ifndef DM365VIDEOOUTPUT_H
#define DM365VIDEOOUTPUT_H

#include "v4l2output.h"
#include "lmmcommon.h"

struct Framecopy_Object;

class DM365VideoOutput : public V4l2Output
{
	Q_OBJECT
public:
	explicit DM365VideoOutput(QObject *parent = 0);
	Lmm::VideoOutput getVideoOutput() { return outputType; }
	void setVideoOutput(Lmm::VideoOutput out);
	void setPixelFormat(int f) { pixelFormat = f; }

	int outputBuffer(RawBuffer buf);
	int start();
	int stop();
signals:
	
public slots:
private:
	Lmm::VideoOutput outputType;
	Framecopy_Object *hFrameCopy;
	bool frameCopyConfigured;
	int pixelFormat;

	void videoCopy(RawBuffer buf, Buffer_Handle dispbuf, Buffer_Handle dmai);
};

#endif // DM365VIDEOOUTPUT_H
