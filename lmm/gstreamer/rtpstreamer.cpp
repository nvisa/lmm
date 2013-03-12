#include "rtpstreamer.h"

#include <gst/app/gstappsrc.h>
#include <gst/gstbuffer.h>
#include <gst/app/gstappsink.h>

#include "debug.h"

#include <QUdpSocket>

static inline bool useH264Parser() { return false; }
static inline bool useAppSink() { return false; }

static GstAppSinkCallbacks sinkCallbacks;

RtpStreamer::RtpStreamer(QObject *parent) :
	AbstractGstreamerInterface(parent)
{
	needData = true;
	dstDataPort = 1234;
	dstControlPort = dstDataPort + 1;
	srcDataPort = 45678;
	srcControlPort = srcDataPort + 1;
	srcDataSock = new QUdpSocket(this);
}

void RtpStreamer::sendBuffer(RawBuffer buf)
{
	if (!isRunning())
		return;

	mInfo("queueing new buffer with size %d, waiting %d", buf.size(), gstBuffers.size());
	GstBuffer *buffer = gst_buffer_new_and_alloc(buf.size());
	memcpy(GST_BUFFER_DATA(buffer), buf.constData(), buf.size());
	float fps = buf.getBufferParameter("fps").toFloat();
	if (fps == 0) {
		mDebug("fps value for buffer is 0, assuming 30 fps");
		fps = 30;
	}
	GST_BUFFER_DURATION(buffer) = 1000000000ll / fps;
	GST_BUFFER_TIMESTAMP(buffer) = GST_BUFFER_DURATION(buffer) * buf.streamBufferNo();
	gstBuffers << buffer;

	pushNextBuffer();
}

int RtpStreamer::startPlayback()
{
	return AbstractGstreamerInterface::startPlayback();
}

int RtpStreamer::stopPlayback()
{
	inputBuffers.clear();
	foreach (GstBuffer *buf, gstBuffers) {
		gst_buffer_unref(buf);
	}
	gstBuffers.clear();

	return AbstractGstreamerInterface::stopPlayback();
}

int RtpStreamer::createElementsList()
{
	GstPipeElement *el;
	el = addElement("appsrc", "rtpsrc");

	if (useH264Parser())
		el = addElement("h264parse", "parser");

	el = addElement("rtph264pay", "payloader");
	el->addParameter("timestamp-offset", rtpTimestampOffset);
	el->addParameter("seqnum-offset", rtpSequenceOffset);

	if (useAppSink()) {
		el = addElement("appsink", "netsink");
		el->addParameter("sync", false);
		el->addParameter("async", false);
	} else {
		el = addElement("udpsink", "netsink");
		el->addParameter("host", dstIp);
		el->addParameter("port", dstDataPort);
		el->addParameter("sync", false);
		el->addParameter("async", false);
	}

	return 0;
}

GstFlowReturn sink_new_preroll(GstAppSink *sink, gpointer user_data)
{
	GstBuffer *buf = gst_app_sink_pull_preroll(sink);
	if (buf) {
		((RtpStreamer *)user_data)->sendData((const char *)GST_BUFFER_DATA(buf), GST_BUFFER_SIZE(buf));
		gst_buffer_unref(buf);
	}
	return GST_FLOW_OK;
}

GstFlowReturn sink_new_buffer(GstAppSink *sink, gpointer user_data)
{
	GstBuffer *buf = gst_app_sink_pull_buffer(sink);
	while (buf) {
		((RtpStreamer *)user_data)->sendData((const char *)GST_BUFFER_DATA(buf), GST_BUFFER_SIZE(buf));
		gst_buffer_unref(buf);
		buf = gst_app_sink_pull_buffer(sink);
	}
	return GST_FLOW_OK;
}

void RtpStreamer::pipelineCreated()
{
	appsrc = getGstPipeElement("rtpsrc");
	gst_app_src_set_caps((GstAppSrc *)appsrc, gst_caps_new_simple("video/x-h264", NULL));
	gst_app_src_set_stream_type((GstAppSrc *)appsrc, GST_APP_STREAM_TYPE_STREAM);

	if (!useAppSink())
		g_object_set(G_OBJECT(getGstPipeElement("netsink")), "sockfd", srcDataSock->socketDescriptor(), NULL);

	if (useAppSink()) {
		sinkCallbacks.eos = NULL;
		sinkCallbacks.new_preroll = sink_new_preroll;
		sinkCallbacks.new_buffer = sink_new_buffer;
		GstAppSink *appsink = (GstAppSink *)getGstPipeElement("netsink");
		gst_app_sink_set_caps(appsink, gst_caps_new_simple("application/x-rtp", NULL));
		gst_app_sink_set_callbacks(appsink, &sinkCallbacks, this, NULL);
	}
}

void RtpStreamer::sendData(const char *data, int size)
{
	if (size < 1500)
		srcDataSock->write(data, size);
	else
		srcDataSock->write(data, 1400);
}

void RtpStreamer::pushNextBuffer()
{
	mInfo("new data request: %d available", gstBuffers.size());
	if (gstBuffers.size() == 0) {
		needData = true;
		return;
	}
	GstBuffer *buffer = gstBuffers.takeFirst();
	if (gst_app_src_push_buffer((GstAppSrc *)appsrc, buffer) != GST_FLOW_OK)
		mDebug("buffer send error");
	needData = false;
}

void RtpStreamer::setDestinationDataPort(int port)
{
	dstDataPort = port;
	srcDataSock->abort();
	srcDataSock->bind();//QHostAddress("192.168.1.196"), srcDataPort);
	srcDataSock->connectToHost(QHostAddress(dstIp), dstDataPort);
	srcDataPort = srcDataSock->localPort();
	srcControlPort = srcDataPort + 1;
}
