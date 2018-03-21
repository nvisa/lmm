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
	int setInputFormat(int infmt);
	int getBufferCount() { return bufferCount; }
	void setBufferCount(int v) { bufferCount = v ? v : bufferCount; }

	static int getFormat(const QString &text);
	static QString getName(int fmt);
signals:
	
public slots:
protected:
	SwsContext *swsCtx;
	LmmBufferPool *pool;
	int inPixFmt;
	int outPixFmt;
	QString mime;
	BaseVideoScaler *scaler;
	int bufferCount;

	void aboutToDeleteBuffer(const RawBufferParameters *params);
};

#endif // FFMPEGCOLORSPACE_H
