#ifndef DM365VIDEOOUTPUT_H
#define DM365VIDEOOUTPUT_H

#include "v4l2output.h"

struct Framecopy_Object;

class DM365VideoOutput : public V4l2Output
{
	Q_OBJECT
public:
	enum videoOutput {
		COMPOSITE,
		COMPONENT
	};
	explicit DM365VideoOutput(QObject *parent = 0);
	void setVideoOutput(videoOutput out) { output = out; }
	void setPixelFormat(int f) { pixelFormat = f; }

	int outputBuffer(RawBuffer buf);
	int start();
	int stop();
signals:
	
public slots:
private:
	videoOutput output;
	Framecopy_Object *hFrameCopy;
	bool frameCopyConfigured;
	int pixelFormat;

	void videoCopy(RawBuffer buf, Buffer_Handle dispbuf, Buffer_Handle dmai);
};

#endif // DM365VIDEOOUTPUT_H
