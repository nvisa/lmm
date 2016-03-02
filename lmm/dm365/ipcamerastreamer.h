#ifndef IPCAMERASTREAMER_H
#define IPCAMERASTREAMER_H

#include <lmm/dmai/dmaibuffer.h>
#include <lmm/players/basestreamer.h>

class BufferQueue;
class TextOverlay;
class JpegEncoder;
class H264Encoder;
class DM365CameraInput;
class DM365VideoOutput;
class DM365DmaCopy;
class SeiInserter;
class RtpTransmitter;

class IPCameraStreamer : public BaseStreamer
{
	Q_OBJECT
public:
	explicit IPCameraStreamer(QObject *parent = 0);

	virtual QList<RawBuffer> getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount);

protected:
	virtual int startStreamer();
	virtual int pipelineOutput(BaseLmmPipeline *pipeline, const RawBuffer &buf);
	QSize getInputSize(int input);

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

#endif // IPCAMERASTREAMER_H
