#include "tk1videostreamer.h"
#include "metadatamanager.h"
#include "tk1omxpipeline.h"
#include "uvcvideoinput.h"
#include "seiinserter.h"
#include "opencv/roi.h"

#include <lmm/debug.h>
#include <lmm/rtp/rtpreceiver.h>
#include <lmm/rtsp/rtspclient.h>
#include <lmm/baselmmpipeline.h>
#include <lmm/rtp/rtptransmitter.h>
#include <lmm/rtsp/basertspserver.h>
#include <lmm/gstreamer/lmmgstpipeline.h>
#include <lmm/pipeline/functionpipeelement.h>
#include <lmm/ffmpeg/ffmpegcolorspace.h>
#include <lmm/ffmpeg/ffcompat.h>

#include <ecl/settings/applicationsettings.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

void asel_via_EGO(unsigned char *buf, int size, int width, int height,int RGB_case,int record_case,int shadow_case,int ill_norm_case,int debug_case,unsigned char* meta,unsigned char* metaPC,unsigned char* dataSize,int tilt_degree, int pan_degree);

void record_video(unsigned char *buf, int size, int width, int height);

void asel_pan(unsigned char *buf, int size, int width, int height,int RGB_case,int shadow_case,int ill_norm_case,int debug_case,unsigned char* meta,unsigned char* metaPC,unsigned char* dataSize,int tilt_degree, int pan_degree);

void asel_via_track(unsigned char *buf, int size, int width, int height,int RGB_case,int record_case,int shadow_case,int ill_norm_case,int debug_case,unsigned char* meta,unsigned char* metaPC,unsigned char* dataSize,int tilt_degree, int pan_degree);

void asel_direct_track(unsigned char *buf, int size, int width, int height,int record_case,int debug_case,unsigned char *meta,unsigned char* metaPC,unsigned char* dataSize,int tilt_degree, int pan_degree);

void asel_via_stage2(unsigned char *buf, int size, int width, int height,int RGB_case,int record_case,int shadow_case,int ill_norm_case,int debug_case,unsigned char* meta,unsigned char* metaPC,unsigned char* dataSize,int tilt_degree, int pan_degree);

void asel_via_stage1(unsigned char *buf, int size, int width, int height,int RGB_case,int record_case,int shadow_case,int ill_norm_case,int debug_case,unsigned char* meta,unsigned char* metaPC,unsigned char* dataSize,int tilt_degree, int pan_degree);

#ifdef CONFIG_VIA
#if 0
#include <src/via_functions_nocv.h>
#include <dlfcn.h>

//extern "C" {

//void asel_via_EGO(unsigned char *buf, int size, int width, int height,int RGB_case,int record_case,int shadow_case,int ill_norm_case,int debug_case,unsigned char* meta,unsigned char* metaPC,unsigned char* dataSize,int tilt_degree, int pan_degree)
//<sextern int asel_via_EGO_test();
typedef AselsanGenericAnalyzer * (*createAnalyzerSign)(AselsanAnalyzers analyzer);

//}
static void init_via()
{
	void *handle = dlopen("viainst/libasel_via.so", RTLD_GLOBAL | RTLD_NOW);
	if (!handle) {
		fDebug("*** error '%s' opening VIA GPU analysis library, GPU functions will not be available. ***", dlerror());
		return;
	}

	createAnalyzerSign cfunc = (createAnalyzerSign)dlsym(handle, "createAnalyzer");
	const char *dlerr = dlerror();
	if (dlerr) {
		fDebug("Error '%s' loading symbol 'createAnalyzer'", dlerr);
		return;
	}
	qDebug("Loaded GPU functions %p", cfunc);
}
#endif
#endif

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
 *  rtpout->setupChannel(ch, "10.50.1.111", 7846, 7847, 7846, 7847, 0x11223344);
 *  rtpout->playChannel(ch);
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
	metaMan = new MetaDataManager();
}

int TK1VideoStreamer::serveRtsp(const QString &ip, const QString &stream)
{
	rtp = new RtpReceiver(this);
	rtpout = new RtpTransmitter(this);
	rtpout->setH264SEIInsertion(true);
	SeiInserter *sei = new SeiInserter();
	sei->setAlarmInformation("/home/ubuntu/sei_alarm_template.xml", 1, 2, 3, 4, 5);

	/* decode pipeline */
	gst1 = new TK1OmxPipeline(this);
	gst1->setPipelineDescription("appsrc name=source ! h264parse ! omxh264dec ! appsink name=sink");
	gst1->getSourceCaps(0)->setMime("video/x-h264,stream-format=byte-stream");
	gst1->doTimestamp(true, 80000);

#if 0
	gst2->setPipelineDescription("appsrc name=source is-live=true do-timestamp=false ! nvhdmioverlaysink sync=false");
	gst2->setPipelineDescription("appsrc name=source is-live=true do-timestamp=false"
								 " ! nveglglessink sync=false"
								 );
#endif
	/* encode pipeline */
	gst2 = new TK1OmxPipeline(this);
	gst2->setPipelineDescription("appsrc name=source is-live=true do-timestamp=false"
								 " ! nvvidconv ! video/x-raw(memory:NVMM)"
								 " ! omxh264enc name=encoder insert-sps-pps=true bitrate=4000000"
								 " ! video/x-h264,stream-format=byte-stream ! appsink name=sink");
#if 0
	gst2->setPipelineDescription("appsrc name=source is-live=true do-timestamp=false ! nvhdmioverlaysink sync=false");
	gst2->setPipelineDescription("appsrc name=source is-live=true do-timestamp=false"
								 " ! nveglglessink sync=false"
								 );
#endif
	gst2->doTimestamp(true, 80000);

	BaseLmmPipeline *p1 = addPipeline();
	p1->append(rtp);
	p1->append(gst1);
	p1->append(newFunctionPipe(TK1VideoStreamer, this, TK1VideoStreamer::processFrame));
	p1->end();

	BaseLmmPipeline *p2 = addPipeline();
	p2->append(gst2);
	p2->append(sei);
	p2->append(rtpout);
	p2->end();

	rtsp = new RtspClient(this);
	rtsp->addSetupTrack("videoTrack", rtp);
	rtsp->setServerUrl(QString("rtsp://%1/%2").arg(ip).arg(stream));

	rtspServer = new BaseRtspServer(this);
	rtspServer->setEnabled(true);
	rtspServer->addStream("stream1", false, rtpout);
	rtspServer->addStream("stream1m",true, rtpout, 15678);
	rtspServer->addMedia2Stream("videoTrack", "stream1", false, rtpout);
	rtspServer->addMedia2Stream("videoTrack", "stream1m", true, rtpout);
	//rtsp->setRtspAuthentication((BaseRtspServer::Auth)s->get("video_encoding.rtsp.auth").toInt());

	return 0;
}

int TK1VideoStreamer::viewSource(const QString &ip, const QString &stream)
{
	rtp = new RtpReceiver(this);
	/* decode pipeline */
	gst1 = new TK1OmxPipeline(this);
	gst1->setPipelineDescription("appsrc name=source ! h264parse ! omxh264dec"
								 //" ! video/x-raw,format=(string)NV12 ! videoconvert"
								 " ! appsink name=sink sync=false");
	gst1->getSourceCaps(0)->setMime("video/x-h264,stream-format=byte-stream");
	gst2 = new TK1OmxPipeline(this);
	gst2->setPipelineDescription("appsrc name=source is-live=true do-timestamp=false"
								 " ! nveglglessink sync=false"
								 );
	gst2->doTimestamp(true, 80000);

	BaseLmmPipeline *p1 = addPipeline();
	p1->append(rtp);
	p1->append(gst1);
//	p1->append((newFunctionPipe(TK1VideoStreamer, this, TK1VideoStreamer::processFrame)));
	p1->end();

	rtsp = new RtspClient(this);
	rtsp->addSetupTrack("videoTrack", rtp);
	rtsp->setServerUrl(QString("rtsp://%1/%2").arg(ip).arg(stream));

	return 0;
}

int TK1VideoStreamer::serveRtp(const QString &ip, const QString &stream, const QString &dstIp, quint16 dstPort)
{
	SeiInserter *sei = new SeiInserter();
	sei->setAlarmInformation("/home/ubuntu/sei_alarm_template.xml", 1, 2, 3, 4, 5);
	rtp = new RtpReceiver(this);
	rtpout = new RtpTransmitter(this);
	rtpout->setH264SEIInsertion(true);
	RtpChannel *ch = rtpout->addChannel();
	rtpout->setupChannel(ch, dstIp, dstPort, dstPort + 1, dstPort, dstPort + 1, 0x11223344);
	rtpout->playChannel(ch);

	/* decode pipeline */
	gst1 = new TK1OmxPipeline(this);
	gst1->setPipelineDescription("appsrc name=source ! h264parse ! omxh264dec ! appsink name=sink");
	gst1->getSourceCaps(0)->setMime("video/x-h264,stream-format=byte-stream");
	gst1->doTimestamp(true, 80000);

	/* encode pipeline */
	gst2 = new TK1OmxPipeline(this);
	gst2->setPipelineDescription("appsrc name=source is-live=true do-timestamp=false ! nvvidconv ! video/x-raw(memory:NVMM) ! omxh264enc name=encoder insert-sps-pps=true bitrate=1000000 ! video/x-h264,stream-format=byte-stream ! appsink name=sink");
	gst2->doTimestamp(true, 80000);

	BaseLmmPipeline *p1 = addPipeline();
	p1->append(rtp);
	p1->append(gst1);
	p1->end();

	BaseLmmPipeline *p2 = addPipeline();
	p2->append(gst2);
	p2->append(sei);
	p2->append(rtpout);
	p2->end();

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
	} else
		//qDebug() << "enc output" << buf.size();
		return 0;
}


int TK1VideoStreamer::processFrame(const RawBuffer &buf)
{
	ApplicationSettings *s = ApplicationSettings::instance();
#ifdef CONFIG_VIA
	int w = buf.constPars()->videoWidth;
	int h = buf.constPars()->videoHeight;
	int rgb = 1;
	int shadow = 0;
	int record = 0;
	int ill = 0;
	int debug = 0;
	uchar meta[4096];
	uchar metaPC[4096]; // not used for EGO
	uchar dataSize[4096]; //not used for EGO
	int tilt_degree = 0; // not used for EGO
	int pan_degree = 0; // not used for EGOs

	/*
	 * asel_via_EGO, and others probably, seems like expecting a grayscale viddeo for rgb=0 case. (buf.size() should be w * h)
	 * At this point, we assume we have an NV12 video so passing first w * h bytes to the tracking function should be valid
	 * function call ;)
	 */

	//asel_pan((uchar *)buf.constData(), bufsize, w, h, rgb, record, shadow, ill, debug, meta, metaPC, dataSize, tilt_degree, pan_degree);
	int bufsize = w * h * 3 / 2;
	if(s->get("config.function_select.asel_via_ego").toBool()) {
		s->saveAll();
		uchar meta[4096];
		asel_via_EGO((uchar *)buf.constData(), bufsize, w, h, rgb, record, shadow, ill, debug, meta, metaPC, dataSize, tilt_degree, pan_degree);
		return 0;
	} else if(s->get("config.function_select.asel_via_track").toBool()) {
		s->saveAll();
		asel_direct_track((uchar *)buf.constData(), bufsize, w, h, rgb, debug, meta, metaPC, dataSize, tilt_degree, pan_degree);
		return 0;
	} else {
		s->saveAll(); // stop
		return 0;
	}
	//metaMan->sendMetaData(metaPC);
#endif
	return 0;
}

int TK1VideoStreamer::viewAnalogGst(const QString &device)
{
	Q_UNUSED(device);
	qDebug() << "view Analog Gst";
	gst1 = new TK1OmxPipeline(this);
	gst1->setPipelineDescription("v4l2src device=/dev/video0 ! video/x-raw,width=704,height=576,format=(string)I420 ! nveglglessink");
	gst1->doTimestamp(8000);

	BaseLmmPipeline *p1 = addPipeline();
	p1->append(gst1);
	p1->end();
	return 0;
}
