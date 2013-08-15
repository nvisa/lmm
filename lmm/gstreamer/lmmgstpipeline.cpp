#include "lmmgstpipeline.h"

#include <lmm/debug.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

#include <errno.h>

static void appSinkEos(GstAppSink *sink, gpointer user_data)
{
	LmmGstPipeline *dec = (LmmGstPipeline *)user_data;
}

static GstFlowReturn appSinkPreroll(GstAppSink *sink, gpointer user_data)
{
	LmmGstPipeline *dec = (LmmGstPipeline *)user_data;
	return (GstFlowReturn)dec->newGstBuffer(gst_app_sink_pull_preroll(sink));
}

static GstFlowReturn appSinkBuffer(GstAppSink *sink, gpointer user_data)
{
	LmmGstPipeline *dec = (LmmGstPipeline *)user_data;
	return (GstFlowReturn)dec->newGstBuffer(gst_app_sink_pull_buffer(sink));
}

static GstFlowReturn appSinkBufferList(GstAppSink *sink, gpointer user_data)
{
	LmmGstPipeline *dec = (LmmGstPipeline *)user_data;
	return GST_FLOW_OK;
}

LmmGstPipeline::LmmGstPipeline(QObject *parent) :
	BaseLmmElement(parent)
{
}

static gboolean busWatch(GstBus *bus, GstMessage *msg, gpointer user)
{
	LmmGstPipeline *dec = (LmmGstPipeline *)user;
	return dec->gstBusFunction(msg);
}

int LmmGstPipeline::start()
{
	GError *err = NULL;
	bin = gst_parse_launch(qPrintable(pipelineDesc), &err);
	if (err) {
		mDebug("error '%s' parsing pipeline", err->message);
		return -EINVAL;
	}
	GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(bin));
	gst_bus_add_watch(bus, busWatch, this);

	appSrc = (GstAppSrc *)gst_bin_get_by_name(GST_BIN(bin), "source");
	appSink = (GstAppSink *)gst_bin_get_by_name(GST_BIN(bin), "sink");

	if (appSrc)
		gst_app_src_set_caps(appSrc, gst_caps_from_string(qPrintable(inputMime)));

	if (appSink) {
		GstAppSinkCallbacks callbacks;
		callbacks.eos = appSinkEos;
		callbacks.new_preroll = appSinkPreroll;
		callbacks.new_buffer= appSinkBuffer;
		callbacks.new_buffer_list = appSinkBufferList;
		gst_app_sink_set_callbacks(appSink, &callbacks, this, NULL);
		gst_app_sink_set_caps(appSink, gst_caps_new_simple(qPrintable(outputMime), NULL));
	}

	if (gst_element_set_state(GST_ELEMENT(bin), GST_STATE_PLAYING) == GST_STATE_CHANGE_ASYNC) {
		mInfo("not waiting async state change");
	}

	return BaseLmmElement::start();
}

int LmmGstPipeline::stop()
{
	mDebug("stopping gstreamer pipeline");
	if (gst_element_set_state(GST_ELEMENT(bin), GST_STATE_NULL) == GST_STATE_CHANGE_ASYNC) {
		mDebug("waiting async state change");
		gst_element_get_state(GST_ELEMENT(bin), NULL, NULL, GST_CLOCK_TIME_NONE);
	}
	return BaseLmmElement::stop();
}

int LmmGstPipeline::addBuffer(RawBuffer buffer)
{
	if (!appSrc) {
		mDebug("error: pipeline has no appsrc element");
		return -ENOENT;
	}
	if (getState() == STOPPED) {
		mDebug("error: element is in wrong state");
		return -EINVAL;
	}
	if (!buffer.size()) {
		mDebug("error: empty buffer is provided");
		return -EINVAL;
	}
	mInfo("sending new buffer to gst pipeline with size %d", buffer.size());
	GstBuffer *buf = gst_buffer_new_and_alloc(buffer.size());
	memcpy(GST_BUFFER_DATA(buf), buffer.constData(), buffer.size());
	if (gst_app_src_push_buffer(appSrc, buf) != GST_FLOW_OK) {
		mDebug("error pushing data to pipeline");
		return -EINVAL;
	}
	mInfo("buffer pushed to gst pipeline");
	receivedBufferCount++;
	releaseInputSem(0);
	return 0;
}

bool LmmGstPipeline::gstBusFunction(GstMessage *msg)
{
	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_ERROR: {
		GError *err = NULL;
		gchar *info = NULL;
		gst_message_parse_error(msg, &err, &info);
		mDebug("pipeline error %s from element %s", err->message, GST_OBJECT_NAME(msg->src));
		mDebug("additional debug info: %s", info ? info : "none");
		g_error_free(err);
		g_free(info);
		stop();
		break;
	}
	case GST_MESSAGE_EOS:
		mDebug("pipeline received EOS");
		stop();
		break;
	case GST_MESSAGE_WARNING: {
		GError *err = NULL;
		gchar *info = NULL;
		gst_message_parse_warning(msg, &err, &info);
		mDebug("pipeline wardning %s from element %s", err->message, GST_OBJECT_NAME(msg->src));
		mDebug("additional debug info: %s", info ? info : "none");
		g_error_free(err);
		g_free(info);
		break;
	}
	case GST_MESSAGE_STATE_CHANGED: {
		GstState oldState, newState, pending;
		gst_message_parse_state_changed(msg, &oldState, &newState, &pending);
		mDebug("'%s' state change completed from %d -> %d, pending %d",
			   GST_OBJECT_NAME(msg->src), oldState, newState, pending);
		if (oldState == GST_STATE_NULL && newState == GST_STATE_PLAYING) {
			mDebug("pipeline started, activating lmm element");
		}
		break;
	}
	case GST_MESSAGE_UNKNOWN:
	case GST_MESSAGE_INFO:
	case GST_MESSAGE_TAG:
	case GST_MESSAGE_BUFFERING:
	case GST_MESSAGE_STATE_DIRTY:
	case GST_MESSAGE_STEP_DONE:
	case GST_MESSAGE_CLOCK_PROVIDE:
	case GST_MESSAGE_CLOCK_LOST:
	case GST_MESSAGE_NEW_CLOCK:
	case GST_MESSAGE_STRUCTURE_CHANGE:
	case GST_MESSAGE_STREAM_STATUS:
	case GST_MESSAGE_APPLICATION:
	case GST_MESSAGE_ELEMENT:
	case GST_MESSAGE_SEGMENT_START:
	case GST_MESSAGE_SEGMENT_DONE:
	case GST_MESSAGE_DURATION:
	case GST_MESSAGE_LATENCY:
	case GST_MESSAGE_ASYNC_START:
	case GST_MESSAGE_ASYNC_DONE:
	case GST_MESSAGE_REQUEST_STATE:
	case GST_MESSAGE_STEP_START:
	case GST_MESSAGE_QOS:
	case GST_MESSAGE_ANY:
		mDebug("new message %s from %s", gst_message_type_get_name(GST_MESSAGE_TYPE(msg)),
			   GST_OBJECT_NAME(msg->src));
		break;
	}
	return true;
}

int LmmGstPipeline::newGstBuffer(GstBuffer *buffer)
{
	RawBuffer buf("unknown", GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
	GstCaps *caps = GST_BUFFER_CAPS(buffer);
	const GstStructure *str = gst_caps_get_structure(caps, 0);
	int w,h;
	gst_structure_get_int(str, "width", &w);
	gst_structure_get_int(str, "height", &h);
	buf.addBufferParameter("width", w);
	buf.addBufferParameter("height", h);
	gst_buffer_unref(buffer);
	outputLock.lock();
	outputBuffers << buf;
	releaseOutputSem(0);
	outputLock.unlock();
	return GST_FLOW_OK;
}