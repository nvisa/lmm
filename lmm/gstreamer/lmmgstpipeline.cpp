#include "lmmgstpipeline.h"

#include <lmm/debug.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

#include <errno.h>
#include <linux/videodev2.h>

static void appSinkEos(GstAppSink *sink, gpointer user_data)
{
	LmmGstPipeline *dec = (LmmGstPipeline *)user_data;
}

static GstFlowReturn appSinkPreroll(GstAppSink *sink, gpointer user_data)
{
	LmmGstPipeline *dec = (LmmGstPipeline *)user_data;
	GstSample *s = gst_app_sink_pull_preroll(sink);
	return (GstFlowReturn)dec->newGstBuffer(gst_sample_get_buffer(s), gst_sample_get_caps(s), sink);
}

static GstFlowReturn appSinkBuffer(GstAppSink *sink, gpointer user_data)
{
	LmmGstPipeline *dec = (LmmGstPipeline *)user_data;
	GstSample *s = gst_app_sink_pull_sample(sink);
	return (GstFlowReturn)dec->newGstBuffer(gst_sample_get_buffer(s), gst_sample_get_caps(s), sink);
}

LmmGstPipeline::LmmGstPipeline(QObject *parent) :
	BaseLmmElement(parent)
{
	debug = false;
	inputTimestamping = false;
}

void LmmGstPipeline::setPipelineDescription(QString desc)
{
	pipelineDesc = desc;

	/* we parse pipeline description and find app[src|sink] elements. */
	QStringList elements = desc.split("!");
	foreach (QString el, elements) {
		if (el.contains(","))
			continue; /* caps filter */
		QStringList flds = el.split(" ", QString::SkipEmptyParts);
		QString type = flds.first();
		QHash<QString, QString> options;

		for (int i = 0; i < flds.size(); i++) {
			if (!flds[i].contains("="))
				continue;
			QStringList vals = flds[i].split("=");
			QString option = vals.first().trimmed();
			QString value = vals.last().trimmed();
			options.insert(option, value);
		}

		if (type == "appsrc")
			addAppSource(options["name"], "");
		else if (type == "appsink")
			addAppSink(options["name"], "");
	}
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

	initElements();

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

int LmmGstPipeline::processBuffer(const RawBuffer &buffer)
{
	GstAppSrc *appSrc = sources.first();
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
	GstMapInfo info;
	gst_buffer_map(buf, &info, GST_MAP_WRITE);
	memcpy(info.data, buffer.constData(), buffer.size());
	gst_buffer_unmap(buf, &info);

	if (inputTimestamping) {
		qint64 count = getOutputQueue(0)->getSentCount();
		GST_BUFFER_PTS(buf) = count * inputFrameDuration;
		GST_BUFFER_DURATION(buf) = inputFrameDuration;
	}

	if (gst_app_src_push_buffer(appSrc, buf) != GST_FLOW_OK) {
		mDebug("error pushing data to pipeline");
		return -EINVAL;
	}
	mInfo("buffer pushed to gst pipeline");
	return 0;
}

void LmmGstPipeline::initElements()
{
	foreach (const QString &name, sourceNames) {
		GstAppSrc *appSrc = (GstAppSrc *)gst_bin_get_by_name(GST_BIN(bin), qPrintable(name));
		if (appSrc) {
			int index = sourceNames.indexOf(name);
			gst_app_src_set_caps(appSrc, sourceCaps[index]->getCaps());
			sources << appSrc;
		}
	}

	foreach (const QString &name, sinkNames) {
		GstAppSink *appSink = (GstAppSink *)gst_bin_get_by_name(GST_BIN(bin), qPrintable(name));
		if (appSink) {
			GstAppSinkCallbacks callbacks;
			callbacks.eos = appSinkEos;
			callbacks.new_preroll = appSinkPreroll;
			callbacks.new_sample = appSinkBuffer;
			gst_app_sink_set_callbacks(appSink, &callbacks, this, NULL);
			sinks << appSink;
		}
	}
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
	default:
		mDebug("new message %s from %s", gst_message_type_get_name(GST_MESSAGE_TYPE(msg)),
			   GST_OBJECT_NAME(msg->src));
		break;
	}
	return true;
}

int LmmGstPipeline::newGstBuffer(GstBuffer *buffer, GstCaps *caps, GstAppSink *sink)
{
	/* create a copy of the buffer */
	GstMapInfo info;
	gst_buffer_map(buffer, &info, GST_MAP_READ);
	RawBuffer buf("unknown", info.data, info.size);
	gst_buffer_unmap(buffer, &info);
	gst_buffer_unref(buffer);

	/* set caps if not already done so */
	float fps = 0.0;
	if (inputTimestamping && inputFrameDuration)
		/* we should adjust caps' frame-rate */
		fps = GST_SECOND / (double)inputFrameDuration;
	int index = sinks.indexOf(sink);
	if (sinkCaps[index]->isEmpty())
		sinkCaps[index]->setCaps(caps, fps);

	BaseGstCaps *bufcaps = sinkCaps[index];
	if (!bufcaps->vid.format.isEmpty()) {
		QString format = bufcaps->vid.format;
		buf.pars()->videoWidth = bufcaps->vid.width;
		buf.pars()->videoHeight = bufcaps->vid.height;
		if (format == "NV12")
			buf.pars()->v4l2PixelFormat = V4L2_PIX_FMT_NV12;
		else
			buf.pars()->v4l2PixelFormat = V4L2_PIX_FMT_UYVY;
	}

	/* forward buffer to our pipeline */
	newOutputBuffer(0, buf);

	return GST_FLOW_OK;
}

void LmmGstPipeline::addAppSource(const QString &name, const QString &mime)
{
	sourceNames << name;
	sourceCaps << new BaseGstCaps(mime);
}

void LmmGstPipeline::addAppSink(const QString &name, const QString &mime)
{
	sinkNames << name;
	sinkCaps << new BaseGstCaps(mime);
}

BaseGstCaps *LmmGstPipeline::getSourceCaps(int index)
{
	return sourceCaps[index];
}

BaseGstCaps * LmmGstPipeline::getSinkCaps(int index)
{
	return sinkCaps[index];
}

int LmmGstPipeline::setSourceCaps(int index, BaseGstCaps *caps)
{
	float fps = 0.0;
	if (inputTimestamping && inputFrameDuration)
		/* we should adjust caps' frame-rate */
		fps = GST_SECOND / (double)inputFrameDuration;
	sourceCaps[index]->setCaps(caps->getCaps(), fps);
	gst_app_src_set_caps(sources[index], caps->getCaps());
	return 0;
}

void LmmGstPipeline::doTimestamp(bool v, qint64 frameDurationUSecs)
{
	 inputTimestamping = v;
	 inputFrameDuration = GST_USECOND * frameDurationUSecs;
}
