#ifndef FFMPEGCOLORSPACE_H
#define FFMPEGCOLORSPACE_H

#include <lmm/baselmmelement.h>

struct SwsContext;
class LmmBufferPool;
class BaseVideoScaler;

class FFmpegColorSpace : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit FFmpegColorSpace(QObject *parent = 0);
	int processBuffer(const RawBuffer &buf);
	int setOutputFormat(int outfmt);
signals:
	
public slots:
protected:
	SwsContext *swsCtx;
	LmmBufferPool *pool;
	int inPixFmt;
	int outPixFmt;
	QString mime;
	BaseVideoScaler *scaler;

	void aboutToDeleteBuffer(const RawBufferParameters *params);
};

#endif // FFMPEGCOLORSPACE_H
