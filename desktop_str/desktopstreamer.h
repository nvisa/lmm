#ifndef DESKTOPSTREAMER_H
#define DESKTOPSTREAMER_H

#include <lmm/players/basestreamer.h>

class x264Encoder;
class X11VideoInput;
class RtpTransmitter;
class SRtpTransmitter;
class FFmpegColorSpace;

class DesktopStreamer : public BaseStreamer
{
	Q_OBJECT
public:
	explicit DesktopStreamer(QObject *parent = 0);

	void setCapture();

signals:

public slots:
protected:
	virtual int pipelineOutput(BaseLmmPipeline *p, const RawBuffer &buf);

	RtpTransmitter *rtp;
	x264Encoder *enc;
	FFmpegColorSpace *cspc;
	X11VideoInput *vin;
};

#endif // DESKTOPSTREAMER_H
