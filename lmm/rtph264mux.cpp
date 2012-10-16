#define __STDC_CONSTANT_MACROS
#include "rtph264mux.h"
#include "streamtime.h"

#include <emdesk/debug.h>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavformat/rtpenc.h>
}

RtpH264Mux::RtpH264Mux(QObject *parent) :
	BaseLmmMux(parent)
{
	loopLatency = 0;
	srcDataPort = 17458;
	srcControlPort = 17489;
	sourceUrlName = "rtp://192.168.1.1:12346?localrtpport=17458?localrtcpport=17489";
	fmt = av_guess_format("rtp", NULL, NULL);
	if (fmt)
		mDebug("rtp encoder found");
}

QString RtpH264Mux::mimeType()
{
	return "application/x-rtp";
}

int RtpH264Mux::start()
{
	sourceUrlName = QString("rtp://%1:%2?localrtpport=%3?localrtcpport=%4")
			.arg(dstIp).arg(dstDataPort).arg(srcDataPort).arg(srcControlPort);
	mDebug("starting RTP streaming to URL %s", qPrintable(sourceUrlName));
	return BaseLmmMux::start();
}

int RtpH264Mux::stop()
{
	return BaseLmmMux::stop();
}

int RtpH264Mux::addBuffer(RawBuffer buffer)
{
	int err = BaseLmmElement::addBuffer(buffer);
	if (err)
		return err;
	RawBuffer buf = nextBuffer();
	while (buf.size()) {
		if (streamTime)
			loopLatency = streamTime->getCurrentTime() - buf.getBufferParameter("captureTime").toInt();
		buf = nextBuffer();
	}
	return 0;
}

QString RtpH264Mux::getSdp()
{
	if (!foundStreamInfo)
		return "";
	char *buff = new char[16384];
	/*
	 * avf_sdp_create doesn't perform any size checking
	 * on input buffer so if we do not provide enough of
	 * it then things can get nasty
	 */
	int err = avf_sdp_create(&context, 1, buff, 16384);
	if (err) {
		mDebug("error %d while getting sdp info.", err);
		delete buff;
		return "";
	}
	QString str(buff);
	delete buff;
	return str;
}

int RtpH264Mux::findInputStreamInfo()
{
	return BaseLmmMux::findInputStreamInfo();
}

int RtpH264Mux::initMuxer()
{
	int err = BaseLmmMux::initMuxer();
	if (err)
		return err;
	context->flags = AVFMT_FLAG_RTP_HINT;
	/*
	 * ssrc is updated in rtp_write_header so at this point
	 * it is safe to read it
	 */
	mDebug("changing RTP private information");
	RTPMuxContext *rtpCtx = (RTPMuxContext *)context->priv_data;
	rtpCtx->seq = 6666;
	rtpCtx->base_timestamp = 1578998879;
	rtpCtx->ssrc = 0x6335D514;
	mDebug("changed RTP private information");
	return 0;
}
