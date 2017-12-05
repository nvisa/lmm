#ifndef VIDEOSCALER_H
#define VIDEOSCALER_H

#include <lmm/baselmmelement.h>

class LmmBufferPool;

class VideoScaler : public BaseLmmElement
{
	Q_OBJECT
public:
	VideoScaler(QObject *parent = NULL);

	void setOutputResolution(int width, int height);
	void setMode(int m) { mode = m; }
protected:
	int processBuffer(const RawBuffer &buf);
	int processScaler(const RawBuffer &buf);
	int processConverter(const RawBuffer &buf);
	void aboutToDeleteBuffer(const RawBufferParameters *params);

	LmmBufferPool *pool;
	int bufferCount;
	QString mime;
	int dstW;
	int dstH;
	int mode;
};

#endif // VIDEOSCALER_H
