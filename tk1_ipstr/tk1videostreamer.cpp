#include "tk1videostreamer.h"
#include "uvcvideoinput.h"
#include "tk1omxpipeline.h"

#include <lmm/debug.h>
#include <lmm/rtp/rtpreceiver.h>
#include <lmm/rtsp/rtspclient.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/rtp/rtptransmitter.h>
#include <lmm/rtsp/basertspserver.h>
#include <lmm/gstreamer/lmmgstpipeline.h>

/**
 * Some example pipelines:
 *
 *		Watch RTP H.264 data:
 *			appsrc name=source ! h264parse ! omxh264dec ! nvhdmioverlaysink sync=false
 *
 *		Encode raw video:
 *			appsrc name=source ! nvvidconv ! video/x-raw(memory:NVMM),width=(int)1920,height=(int)1080,format=(string)I420,framerate=(fraction)30/1 ! omxh264enc ! appsink name=sink;
 *
 * You can use following for RTP streaming (no rtsp):
 *	RtpChannel *ch = rtpout->addChannel();
 * rtpout->setupChannel(ch, "10.50.1.111", 7846, 7847, 7846, 7847, 0x11223344);
 * rtpout->playChannel(ch);
 */
TK1VideoStreamer::TK1VideoStreamer(QObject *parent)
	: BaseStreamer(parent)
{
#if 0
	vin = new UvcVideoInput(this);
	vin->setParameter("videoWidth", 2596);
	//vin->setParameter("videoWidth", 1920);
	vin->setParameter("videoHeight", 1120);
	//vin->setParameter("videoHeight", 1080);
	//vin->start();
#endif
}

int TK1VideoStreamer::serveRtsp(const QString &ip, const QString &stream)
{
	rtp = new RtpReceiver(this);
	rtpout = new RtpTransmitter(this);

	/* decode pipeline */
	gst1 = new TK1OmxPipeline(this);
	gst1->setPipelineDescription("appsrc name=source ! h264parse ! omxh264dec ! appsink name=sink");
	gst1->getSourceCaps(0)->setMime("video/x-h264,stream-format=byte-stream");
	gst1->doTimestamp(true, 80000);

	/* encode pipeline */
	gst2 = new TK1OmxPipeline(this);
	gst2->setPipelineDescription("appsrc name=source is-live=true do-timestamp=false ! nvvidconv ! video/x-raw(memory:NVMM) ! omxh264enc name=encoder insert-sps-pps=true bitrate=4000000 ! video/x-h264,stream-format=byte-stream ! appsink name=sink");
	gst2->doTimestamp(true, 80000);

	BaseLmmPipeline *p1 = addPipeline();
	p1->append(rtp);
	p1->append(gst1);
	p1->end();

	BaseLmmPipeline *p2 = addPipeline();
	p2->append(gst2);
	p2->append(rtpout);
	p2->end();

	rtsp = new RtspClient(this);
	rtsp->addSetupTrack("videoTrack", rtp);
	rtsp->setServerUrl(QString("rtsp://%1/%2").arg(ip).arg(stream));

	rtspServer = new BaseRtspServer(this);
	rtspServer->setEnabled(true);
	rtspServer->addStream("stream1", false, rtpout);
	rtspServer->addMedia2Stream("videoTrack", "stream1", false, rtpout);
	//rtsp->setRtspAuthentication((BaseRtspServer::Auth)s->get("video_encoding.rtsp.auth").toInt());

	return 0;
}

int TK1VideoStreamer::viewSource(const QString &ip, const QString &stream)
{
	rtp = new RtpReceiver(this);

	/* decode pipeline */
	gst1 = new TK1OmxPipeline(this);
	gst1->setPipelineDescription("appsrc name=source ! h264parse ! omxh264dec ! nvhdmioverlaysink sync=false");
	gst1->getSourceCaps(0)->setMime("video/x-h264,stream-format=byte-stream");

	BaseLmmPipeline *p1 = addPipeline();
	p1->append(rtp);
	p1->append(gst1);
	p1->end();

	rtsp = new RtspClient(this);
	rtsp->addSetupTrack("videoTrack", rtp);
	rtsp->setServerUrl(QString("rtsp://%1/%2").arg(ip).arg(stream));

	return 0;
}

int TK1VideoStreamer::pipelineOutput(BaseLmmPipeline *p, const RawBuffer &buf)
{
	if (p == getPipeline(0)) {
		BaseGstCaps *c = gst1->getSinkCaps(0);
		if (gst2->getSourceCaps(0)->isEmpty()) {
			qDebug() << "setting source caps";
			gst2->setSourceCaps(0, c);
		}
		gst2->addBuffer(0, buf);
		return 0;
	}
	return 0;
}

