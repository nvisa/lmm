#ifndef OMXSCALER_H
#define OMXSCALER_H

#include "baseomxelement.h"

#include <QMap>
#include <QPair>
#include <QSize>

typedef void* OMX_HANDLETYPE;
struct OMX_CALLBACKTYPE;
struct OMX_BUFFERHEADERTYPE;

class OmxScaler : public BaseOmxElement
{
	Q_OBJECT
public:
	explicit OmxScaler(QObject *parent = 0);
	int setInputFrameSize(QSize sz);
	int setOutputFrameSize(QSize sz);
	int execute();
	int startComponent();
	int stopComponent();
	void setVideoStride(int val) { videoStride = val; }
	int getVideoStride() { return videoStride; }
	void setDecoderBuffers(QList< QPair<OMX_BUFFERHEADERTYPE *, int> > bufs) { decoderBuffers = bufs; }
	const QList< QPair<OMX_BUFFERHEADERTYPE *, int> > getScalarBuffers();
signals:
	
private slots:
	void handleDispBuffer(OMX_BUFFERHEADERTYPE *omxBuf);
	void handleInputBufferDone(OMX_BUFFERHEADERTYPE *omxBuf);
protected:
	OMX_HANDLETYPE handleScalar;
	OMX_BUFFERHEADERTYPE *scInBufs[32];
	OMX_BUFFERHEADERTYPE *scOutBufs[32];
	QMap<OMX_BUFFERHEADERTYPE *, OMX_BUFFERHEADERTYPE *> bufMap;
	QMap<OMX_BUFFERHEADERTYPE *, RawBuffer> rawBufMap;
	QList< QPair<OMX_BUFFERHEADERTYPE *, int> > decoderBuffers;
	//QWaitCondition wci;
	int width;
	int height;
	int outputWidth;
	int outputHeight;
	int outputBufferSize;
	int videoStride;

	int processBuffer(const RawBuffer &buf);
	int setOmxScalarParams();
	OMX_HANDLETYPE getCompHandle() { return handleScalar; }
};

#endif // OMXSCALER_H
