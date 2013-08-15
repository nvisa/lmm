#ifndef OMXDISPLAYOUTPUT_H
#define OMXDISPLAYOUTPUT_H

#include "baseomxelement.h"

#include <QSize>

class OmxDisplayOutput : public BaseOmxElement
{
	Q_OBJECT
public:
	explicit OmxDisplayOutput(QObject *parent = 0);
	int setFrameSize(QSize sz);
	int setDisplaySize(QSize sz);
	int displayBlocking();
	int display(RawBuffer buf);
	void setScalarBuffers(QList< QPair<OMX_BUFFERHEADERTYPE *, int> > bufs) { scalarBuffers = bufs; }
protected:
	int startComponent();
	int stopComponent();
protected slots:
	void handleDispBuffer(OMX_BUFFERHEADERTYPE *omxBuf);
	void handleInputBufferDone(OMX_BUFFERHEADERTYPE *omxBuf);
protected:
	int setOmxDisplayParams();
	int execute();
	OMX_HANDLETYPE getCompHandle() { return handleDisp; }

	OMX_HANDLETYPE handleDisp;
	/* controller handle is to only control display, no buffers and other stuff at all */
	OMX_HANDLETYPE handleCtrl;
	OMX_BUFFERHEADERTYPE *dispInBufs[32];
	QMap<OMX_BUFFERHEADERTYPE *, OMX_BUFFERHEADERTYPE *> bufMap;
	QMap<OMX_BUFFERHEADERTYPE *, RawBuffer> rawBufMap;
	QList< QPair<OMX_BUFFERHEADERTYPE *, int> > scalarBuffers;
	int width;
	int height;
	int dispWidth;
	int dispHeight;
};

#endif // OMXDISPLAYOUTPUT_H
