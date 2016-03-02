#ifndef SIMPLERTPSTREAMER_H
#define SIMPLERTPSTREAMER_H

#include <lmm/players/basestreamer.h>

class FileOutput;
class H264Encoder;
class SeiInserter;
class RtpPacketizer;
class DM365CameraInput;

class SimpleRtpStreamer : public BaseStreamer
{
	Q_OBJECT
public:
	explicit SimpleRtpStreamer(QObject *parent = 0);
	virtual int start();
protected:
	virtual int startStreamer();
	virtual int pipelineOutput(BaseLmmPipeline *pipeline, const RawBuffer &buf);
	QSize getInputSize(int input);

	/* elements */
	RtpPacketizer *rtp;
	DM365CameraInput *camIn;
	H264Encoder *enc264High;
	SeiInserter *seiInserterHigh;
	FileOutput *fout;
};

#endif // SIMPLERTPSTREAMER_H
