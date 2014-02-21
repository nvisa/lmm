#ifndef FFMPEGCOLORSPACE_H
#define FFMPEGCOLORSPACE_H

#include <lmm/baselmmelement.h>

struct SwsContext;
class LmmBufferPool;

class FFmpegColorSpace : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit FFmpegColorSpace(QObject *parent = 0);
	int processBuffer(const RawBuffer &buf);
	int convertToRgb();
	int convertToGray();
signals:
	
public slots:
protected:
	SwsContext *swsCtx;
	LmmBufferPool *pool;
	QMap<int, RawBuffer> poolBuffers;
	int inPixFmt;
	int outPixFmt;
	QString mime;
	int srcStride[3];

	void aboutToDeleteBuffer(const RawBufferParameters *params);
};

#endif // FFMPEGCOLORSPACE_H
