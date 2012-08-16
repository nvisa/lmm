#include "rtpstreamer.h"

#include <gst/app/gstappsrc.h>
#include <gst/gstbuffer.h>

#include <emdesk/debug.h>

#include <QUdpSocket>

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

	//inputBuffers << buf;
	mInfo("queueing new buffer with size %d", buf.size());
	GstBuffer *buffer = gst_buffer_new_and_alloc(buf.size());
	memcpy(GST_BUFFER_DATA(buffer), buf.constData(), buf.size());
	int fps = buf.getBufferParameter("fps").toInt();
	GST_BUFFER_DURATION(buffer) = 1000000000ll / fps;
	GST_BUFFER_TIMESTAMP(buffer) = GST_BUFFER_DURATION(buffer) * buf.streamBufferNo();
	gstBuffers << buffer;

	//if (needData)
		pushNextBuffer();
}

int RtpStreamer::startPlayback()
{
	return AbstractGstreamerInterface::startPlayback();
}

int RtpStreamer::stopPlayback()
{
	qDebug() << "stopping playback";
	inputBuffers.clear();
	return AbstractGstreamerInterface::stopPlayback();
}

int RtpStreamer::createElementsList()
{
	GstPipeElement *el;
	el = addElement("appsrc", "rtpsrc");

	el = addElement("h264parse", "parser");
	el = addElement("rtph264pay", "payloader");
	el->addParameter("timestamp-offset", rtpTimestampOffset);
	el->addParameter("seqnum-offset", rtpSequenceOffset);

	el = addElement("udpsink", "netsink");
	el->addParameter("host", dstIp);
	el->addParameter("port", dstDataPort);
	el->addParameter("sync", false);
	el->addParameter("async", false);

	/*el = addElement("filesink", "filesink");
	el->addParameter("location", "test.h264");
	el->addParameter("sync", false);
	el->addParameter("async", false);*/
	return 0;
}

void appsrc_need_data(GstAppSrc *src, guint length, gpointer user_data)
{
	((RtpStreamer *)(user_data))->pushNextBuffer();
}
GstAppSrcCallbacks mycallbacks;

void RtpStreamer::pipelineCreated()
{
	appsrc = getGstPipeElement("rtpsrc");
	gst_app_src_set_caps((GstAppSrc *)appsrc, gst_caps_new_simple("video/x-h264", NULL));
	gst_app_src_set_stream_type((GstAppSrc *)appsrc, GST_APP_STREAM_TYPE_STREAM);
	mycallbacks.need_data = &appsrc_need_data;
	mycallbacks.enough_data = NULL;
	mycallbacks.seek_data = NULL;
	//gst_app_src_set_callbacks((GstAppSrc *)appsrc, &mycallbacks, this,  NULL);

	g_object_set(G_OBJECT(getGstPipeElement("netsink")), "sockfd", srcDataSock->socketDescriptor(), NULL);
}

void RtpStreamer::pushNextBuffer()
{
#if 0
	if (inputBuffers.size() == 0) {
		needData = true;
		return;
	}
	RawBuffer buf = inputBuffers.takeFirst();
	mDebug("sending buffer, %d bytes", buf.size());
	GstBuffer *buffer = gst_buffer_new_and_alloc(buf.size());
	memcpy(GST_BUFFER_DATA(buffer), buf.constData(), buf.size());
	int fps = buf.getBufferParameter("fps").toInt();
	GST_BUFFER_DURATION(buffer) = 1000000000ll / fps;
	GST_BUFFER_TIMESTAMP(buffer) = GST_BUFFER_DURATION(buffer) * buf.streamBufferNo();
	if (gst_app_src_push_buffer((GstAppSrc *)appsrc, buffer) != GST_FLOW_OK)
		mDebug("buffer send error");
	needData = false;
#else
	mInfo("new data request: %d available", gstBuffers.size());
	if (gstBuffers.size() == 0) {
		needData = true;
		return;
	}
	GstBuffer *buffer = gstBuffers.takeFirst();
	if (gst_app_src_push_buffer((GstAppSrc *)appsrc, buffer) != GST_FLOW_OK)
		mDebug("buffer send error");
	needData = false;
#endif
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
