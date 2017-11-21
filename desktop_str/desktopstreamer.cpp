#include "desktopstreamer.h"
#include "x11videoinput.h"

#include <lmm/debug.h>
#include <lmm/fileoutput.h>
#include <lmm/x264encoder.h>
#include <lmm/qtvideooutput.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/rtp/rtptransmitter.h>
#include <lmm/rtp/srtptransmitter.h>
#include <lmm/rtsp/basertspserver.h>
#include <lmm/ffmpeg/ffmpegcolorspace.h>

#include <linux/videodev2.h>

extern "C" {
	#include <libavformat/avformat.h>
}

DesktopStreamer::DesktopStreamer(QObject *parent)
	: BaseStreamer(parent)
{

}

void DesktopStreamer::setCapture()
{
	rtp = new SRtpTransmitter(this);

	enc = new x264Encoder(this);
	enc->setPixelFormat(V4L2_PIX_FMT_YUV420);
	enc->setVideoResolution(QSize(1920, 1080));

	cspc = new FFmpegColorSpace(this);
	cspc->setOutputFormat(AV_PIX_FMT_YUV420P);
	cspc->setInputFormat(AV_PIX_FMT_RGBA);

	//FileOutput *fout = new FileOutput(this);
	//fout->setFileName("test.h264");
	vin = new X11VideoInput(this);

	BaseLmmPipeline *p1 = addPipeline();
	p1->append(vin);
	p1->append(cspc);
	p1->append(enc);
	p1->append(rtp);

	//p1->append(fout);
	//p1->append(new QtVideoOutput(this));

	p1->end();

	BaseRtspServer *rtspServer = new BaseRtspServer(this, 8554);
	rtspServer->setEnabled(true);
	rtspServer->addStream("stream1", false, rtp);
	rtspServer->addMedia2Stream("videoTrack", "stream1", false, rtp);
}

int DesktopStreamer::pipelineOutput(BaseLmmPipeline *p, const RawBuffer &buf)
{
	//qDebug() << p << buf.size();
	if (vin->getOutputQueue(0)->getRateLimit() == ElementIOQueue::LIMIT_NONE)
		vin->getOutputQueue(0)->setRateLimitInterval(40);
	//qDebug() << vin->getOutputQueue(0)->getFps() << rtp->getOutputQueue(0)->getBitrate();
	return 0;
}

