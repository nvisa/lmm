#ifndef GENERICSTREAMER_H
#define GENERICSTREAMER_H

#include <lmm/dm365/ipcamerastreamer.h>

class BufferQueue;
class TextOverlay;
class JpegEncoder;
class H264Encoder;
class DM365CameraInput;
class DM365VideoOutput;
class DM365DmaCopy;
class SeiInserter;
class RtpTransmitter;

class GenericStreamer : public BaseStreamer
{
	Q_OBJECT
public:
	explicit GenericStreamer(QObject *parent = 0);

signals:

public slots:
protected:
	/* elements */
	DM365CameraInput *camIn;
	H264Encoder *enc264High;
	H264Encoder *enc264Low;
	DM365VideoOutput *videoOut;
	TextOverlay *textOverlay;
	TextOverlay *textOverlay2;
	DM365DmaCopy *cloner;
	BufferQueue *rawQueue;
	BufferQueue *rawQueue2;
	BufferQueue *h264Queue;
	JpegEncoder *encJpegHigh;
	JpegEncoder *encJpegLow;
	SeiInserter *seiInserterHigh;
	RtpTransmitter *rtpLow;
	RtpTransmitter *rtpHigh;
};

#endif // GENERICSTREAMER_H
