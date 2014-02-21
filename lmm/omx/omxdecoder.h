#ifndef OMXDECODER_H
#define OMXDECODER_H

#include "baseomxelement.h"

#include <QSize>
#include <QMutex>

typedef void* OMX_HANDLETYPE;
struct OMX_CALLBACKTYPE;
struct OMX_BUFFERHEADERTYPE;

class OmxDecoder : public BaseOmxElement
{
	Q_OBJECT
public:
	explicit OmxDecoder(QObject *parent = 0);

	int setFrameSize(QSize sz);
	void setTargetFps(int fps) { targetFps = fps; }
	int execute();
	int getVideoStride() { return videoStride; }
	const QList< QPair<OMX_BUFFERHEADERTYPE *, int> > getDecoderBuffers();
protected:
	int processBuffer(const RawBuffer &buf);
	int startComponent();
	int stopComponent();
	OMX_HANDLETYPE getCompHandle() { return handleDec; }
private slots:
	void handleDispBuffer(OMX_BUFFERHEADERTYPE *omxBuf);
	void handleInputBufferDone(OMX_BUFFERHEADERTYPE *omxBuf);
private:
	int setOmxDecoderParams();

	OMX_HANDLETYPE handleDec;
	int codingType;
	int width;
	int height;
	int videoStride;
	int outputBufferSize;
	int targetFps;

	QWaitCondition wci;
	OMX_BUFFERHEADERTYPE *decInBufs[32];
	OMX_BUFFERHEADERTYPE *decOutBufs[32];
};

#endif // OMXDECODER_H
