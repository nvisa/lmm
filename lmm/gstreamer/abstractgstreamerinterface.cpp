/*
 * Copyright (C) 2010 Yusuf Caglar Akyuz
 *
 * Her hakki Bilkon Ltd. Sti.'ye  aittir.
 */

#include "abstractgstreamerinterface.h"
#include <QCoreApplication>
#include <QStringList>
#include <QTime>

#include <emdesk/debug.h>

void demux_pad_removed(GstElement *, GstPad *, gpointer)
{

}

void demux_pad_added(GstElement *el, GstPad *pad, gpointer data)
{
	fDebug("new pad added: %s:%s", el->object.name, pad->object.name);
	GstPipeElement *pipeEl = (GstPipeElement *)data;
	int pos = pipeEl->getSourceElementNames().indexOf(el->object.name);
	if (pos < 0) {
		 if (pipeEl->getSourceElementNames().size()) {
			fDebug("pad %s:%s does not exist in link list of element %s, I'm skipping", el->object.name,
			   pad->object.name, GST_OBJECT_NAME(pipeEl->getGstElement()));
			return;
		 }
		 AbstractGstreamerInterface::link_to_static_sink_pad(pipeEl->getGstElement(),
															 pad);
		 return;
	}
	if (pipeEl->getSourceElementPads()[pos] != pad->object.name) {
		fDebug("this is not a desired link");
		return;
	}
	QString destPadName = pipeEl->getElementPads()[pos];
	if (destPadName == "")
		destPadName = "sink";
	GstPad *destPad = gst_element_get_static_pad(pipeEl->getGstElement(), qPrintable(destPadName));
	if (!destPad) {
		fDebug("No %s pad exists in element %s, skipping linking", qPrintable(destPadName)
			   , pipeEl->getGstElement()->object.name);
		return;
	}

	if (!GST_PAD_IS_SRC(pad))
		return;
#if GST_CHECK_VERSION(0, 10, 26)
	GstCaps *srcCaps = gst_pad_get_caps_reffed(pad);
	GstCaps *sinkCaps = gst_pad_get_caps_reffed(destPad);
#else
	GstCaps *srcCaps = gst_pad_get_allowed_caps(pad);
	GstCaps *sinkCaps = gst_pad_get_allowed_caps(destPad);
#endif
	if (gst_caps_can_intersect(srcCaps, sinkCaps) == FALSE) {
		if (srcCaps)
			g_object_unref(srcCaps);
		if (sinkCaps)
			g_object_unref(sinkCaps);
		GstObject *obj = gst_pad_get_parent(destPad);
		fDebug("pad %s:%s cannot be linked to %s:%s, I'm skipping", el->object.name,
			   pad->object.name, GST_OBJECT_NAME(obj), destPad->object.name);
		gst_object_unref(obj);
		return;
	}

#if !GST_CHECK_VERSION(0, 10, 26)
	gst_object_unref(srcCaps);
	gst_object_unref(sinkCaps);
#endif

	if (gst_pad_is_linked(pad)) {
		GstPad *tmp = gst_pad_get_peer(pad);
		gst_pad_unlink(pad, tmp);
	}

	if (gst_pad_is_linked(destPad)) {
		GstPad *tmp = gst_pad_get_peer(destPad);
		gst_pad_unlink(tmp, destPad);
	}

	if (gst_pad_link(pad, destPad) != GST_PAD_LINK_OK)
		fDebug("cannot link %s:%s to %s", el->object.name,
			   pad->object.name, destPad->object.name);

	GstObject *obj = gst_pad_get_parent(destPad);
	fInfo("new %s pad(%s) linked to %s:%s", el->object.name,
		  pad->object.name, GST_OBJECT_NAME(obj), destPad->object.name);
	gst_object_unref(obj);
}

gboolean handleBusMessage(GstBus *, GstMessage *msg, gpointer data)
{
	gchar  *debug;
	GError *error;
	AbstractGstreamerInterface *in = (AbstractGstreamerInterface *)data;
	switch (GST_MESSAGE_TYPE (msg)) {

	case GST_MESSAGE_EOS:
		g_print ("End of stream\n");
		in->stopPlayback();
		break;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &error, &debug);
		g_free (debug);

		g_printerr ("Error: %s\n", error->message);
		g_error_free (error);
		in->stopPlayback();
		break;
	case GST_MESSAGE_STATE_CHANGED:
		if (GST_MESSAGE_SRC(msg) != GST_OBJECT_CAST(in->pipeline))
			break;
		GstState o, n, p;
		gst_message_parse_state_changed(msg, &o, &n, &p);
		in->pipelineStateChanged(n);
		break;
	case GST_MESSAGE_WARNING:
		gst_message_parse_warning (msg, &error, &debug);
		g_free (debug);

		g_print ("Warning: %s\n", error->message);
		g_error_free (error);
		break;
#ifdef GST_MESSAGE_QOS
	case GST_MESSAGE_QOS:
		break;
#endif
	case GST_MESSAGE_INFO:
	{
		GError *gerror;
		gchar *debug;
		gchar *name = gst_object_get_path_string (GST_MESSAGE_SRC (msg));

		gst_message_parse_info (msg, &gerror, &debug);
		if (debug) {
			g_print("INFO:\n%s\n", debug);
		}
		g_error_free (gerror);
		g_free (debug);
		g_free (name);
		break;
	}
	case GST_MESSAGE_APPLICATION:
		in->newApplicationMessageReceived();
		break;
	case GST_MESSAGE_NEW_CLOCK:
		GstClock *clock;
		gst_message_parse_new_clock(msg, &clock);
		g_print ("New clock: %s\n", GST_OBJECT_NAME(clock));
		break;
	case GST_MESSAGE_ELEMENT:
	{
		const char *m = gst_structure_get_name(msg->structure);
		g_print ("New element message: %s\n", m);
		QString elmsg(m);
		if (in->newElementMessageReceived(elmsg))
			in->stopPlayback();
		break;
	}
	default:
		g_print("message: %d: %s\n", GST_MESSAGE_TYPE (msg), GST_MESSAGE_TYPE_NAME (msg));
		break;
	}

	/* TODO: unref message */
	return TRUE;
}

gboolean padWatcher(GstPad *, GstMiniObject *obj, gpointer user_data)
{
	GstBuffer *buf = (GstBuffer *)obj;
	GstPipeElement *el = (GstPipeElement *)user_data;
	gboolean ret = TRUE;
	el->incrementBufferCount();
	if (el->parent()->padProbesInstalled)
		ret = el->parent()->newPadBuffer(buf, el) ? TRUE : FALSE;
	el->parent()->sync();
	return ret;
}

GstPipeElement * AbstractGstreamerInterface::findElementByNodeName(QString name)
{
	for (int i = 0; i < pipeElements.size(); ++i)
		if (pipeElements[i]->nodeName() == name)
			return pipeElements[i];
	return NULL;
}

/**
  * This function will try to link 2 elements using a pad name.
  *
  * This function will search existing pads in the source element for
  * the given pad name and will link it if such a pad exists. Old links
  * are un-linked if they exist.
  *
  * \param decoder Sink element to link
  * \param demux Source element to link
  * \param Pad name of an exisiting pad in source element
  */
void AbstractGstreamerInterface::link_to_static_sink_pad(GstElement *decoder, GstElement *demux, const char * padName)
{
	int ret;
	GstPad *decPad = gst_element_get_static_pad(decoder, "sink");
	if (!decPad) {
		fDebug("unable get sink pad from %s element with pad name %s", decoder->object.name, padName);
		return;
	}
	GstPad *pad = gst_element_get_static_pad(demux, padName);
	if (pad) {
		if (gst_pad_is_linked(decPad)) {
			/* First we need to cut previous link */
			GstPad *peer = gst_pad_get_peer(decPad);
			gst_pad_unlink(peer, decPad);
			gst_object_unref(peer);
		}
		if (gst_pad_is_linked(pad)) {
			/* First we need to cut previous link */
			GstPad *peer = gst_pad_get_peer(pad);
			gst_pad_unlink(decPad, peer);
			gst_object_unref(peer);
		}
		ret = gst_pad_link(pad, decPad);
		if (ret != GST_PAD_LINK_OK)// && ret != GST_PAD_LINK_NOFORMAT && ret != GST_PAD_LINK_WAS_LINKED)
			fDebug("unable to link demux and decoder elements with pads. err is %d", ret);
		else
			fInfo("pad %s:%s is successfully linked to %s:%s", demux->object.name, padName,
				  decoder->object.name, decPad->object.name);
		gst_object_unref(pad);
	} else
		fInfo("unable to find an existing pad for %s, it will be linked when the pad is added", padName);

	gst_object_unref(decPad);
}

/**
  * This function will try to link 1 element with a given pad.
  *
  * This function will link the given sink element to the given pad.
  * Old links are un-linked if they exist.
  *
  * \param decoder Sink element to link
  * \param pad Pad of the source element to link
  */
void AbstractGstreamerInterface::link_to_static_sink_pad(GstElement *decoder, GstPad *pad)
{
	GstPad *decPad = gst_element_get_static_pad(decoder, "sink");
	if (!decPad) {
		fDebug("unable get sink pad from decoder element");
		return;
	}
	if (gst_pad_is_linked(decPad)) {
		/* First we need to cut previous link */
		fInfo("un-linking previous demux-decoder link");
		GstPad *peer = gst_pad_get_peer(decPad);
		gst_pad_unlink(peer, decPad);
		gst_object_unref(peer);
	}
	int ret = gst_pad_link(pad, decPad);
	if (ret != GST_PAD_LINK_OK)
		fDebug("unable to link demux and decoder elements with pads. err is %d", ret);

	gst_object_unref(decPad);
}

AbstractGstreamerInterface::AbstractGstreamerInterface(QObject *parent) :
	QObject(parent)
{
	pipelineRunning = false;
	pipelinePaused = false;
	pipeline = NULL;
	positionQuery = NULL;
	padProbesInstalled = false;
	startSyncWait = false;
}

AbstractGstreamerInterface::~AbstractGstreamerInterface()
{
}

int AbstractGstreamerInterface::startPlayback()
{
	if (pipelineRunning)
		return -EBUSY;

	QTime t;
	gstPipelineState = GST_STATE_NULL;
	mDebug("start");
	if (!pipeline) {
		int res = createElementsList();
		if (res)
			return res;
		res = createPipeline();
		mInfo("element list created, createPipeline() returned %d", res);
		if (res)
			return res;
		mInfo("pipeline created");
		pipelineCreated();
	}

	elementCount = pipeElements.size();
	playbackDuration = 0;
	lastPipelineError = 0;
	lastPipelineErrorMsg = "";

	mDebug("setting pipeline to paused");
	GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PAUSED);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		mDebug("pipeline does not want to pause, stopping.");
		stopPlayback();
		return -EPERM;
	} else if (ret == GST_STATE_CHANGE_ASYNC) {
		mDebug("waiting pipeline to go to paused state");
		t.start();
		while (gstPipelineState != GST_STATE_PAUSED) {
			QCoreApplication::processEvents();
			if (t.elapsed() > 10000)
				break;
		}
		if (gstPipelineState != GST_STATE_PAUSED) {
			mDebug("pipeline does not go to pause, current state is %d stopping.", gstPipelineState);
			stopPlayback();
			return -EPERM;
		}
	}

	mDebug("setting pipeline to playing");
	ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		mDebug("pipeline does not go to playing, stopping.");
		stopPlayback();
		return -EPERM;
	}

	mDebug("end");
	return 0;
}

int AbstractGstreamerInterface::stopPlayback()
{
	if (!pipeline)
		return 0;

	mDebug("start");
	/* remove probes first if they are installed */
	removePadProbes();

	/* transition to NULL state can never be async */
	gst_element_set_state (pipeline, GST_STATE_NULL);
	mDebug("state change done");
	destroyPipeline();
	mDebug("pipeline destroyed");
	qDeleteAll(pipeElements);
	pipeElements.clear();
	pipelineRunning = false;
	mDebug("end");

	return 0;
}

void AbstractGstreamerInterface::syncWait()
{
	installFlowPadProbe("sink");
	startSyncWait = true;
	sSync.acquire(1);
	removeFlowPadProbes();
}

int AbstractGstreamerInterface::pausePlayback()
{
	if (pipelinePaused)
		return 0;
	mDebug("start");
	if (gst_element_set_state (pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_ASYNC) {
		mDebug("async stop, waiting infinitely");
		if (gst_element_get_state(pipeline, NULL, NULL, GST_SECOND) == GST_STATE_CHANGE_ASYNC) {
			mDebug("Resetting");
			stopPlayback();
			startPlayback();
			gst_element_set_state (pipeline, GST_STATE_PAUSED);
			gst_element_get_state(pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
		}
	}
	pipelinePaused = true;
	mDebug("end");

	return 0;
}

GstPipeElement * AbstractGstreamerInterface::addElement(QString elementName, QString nodeName, QString srcElement)
{
	GstPipeElement *el = new GstPipeElement(elementName, nodeName, this);
	/*
	 * If srcElement is not given then element will link to previous element
	 * so we add nothing
	 */
	if (srcElement != "")
		el->addSourceLink(srcElement, "", GstPipeElement::PAD_ALWAYS, "",
						  GstPipeElement::PAD_ALWAYS);
	pipeElements.append(el);
	return el;
}

int AbstractGstreamerInterface::elementIndex(QString name)
{
	for (int i = 0; pipeElements.size(); ++i)
		if (pipeElements[i]->nodeName() == name)
			return i;
	return -1;
}

GstElement * AbstractGstreamerInterface::getGstElement(QString nodeName)
{
	return gstPipeElements[nodeName];
}

GstPipeElement::GstPipeElement(QString gstElement, QString nodeName, QObject *parent)
	: QObject(parent)
{
	elementName = gstElement;
	name = nodeName;
	lastBufferCount = 0;
	bufferCount = 0;
	doNotLink = false;
	watchElement = false;
	parentInterface = (AbstractGstreamerInterface *)parent;
}

bool GstPipeElement::isCapsFilter()
{
	return elementName.contains("/");
}

GstPipeElement * AbstractGstreamerInterface::findPreviousElementToLink(int elementPos, int linkNo)
{
	GstPipeElement *prevEl = NULL;
	GstPipeElement *el = pipeElements.at(elementPos);
	QStringList linkElementNames = el->getSourceElementNames();

	/* Find which element to link */
	if (!linkElementNames.size()) {
		prevEl = pipeElements.at(elementPos - 1);
		if (prevEl->isCapsFilter())
			prevEl = pipeElements.at(elementPos - 2);
	} else {
		prevEl = findElementByNodeName(linkElementNames[linkNo]);
	}

	return prevEl;
}

QString AbstractGstreamerInterface::findPadToLink(GstElement *node, bool findSinkPads, QString padName)
{
	GstIterator *it;
	gboolean done = FALSE;
	GstPad *pad;
	QString name;
	if (findSinkPads)
		it = gst_element_iterate_sink_pads(node);
	else
		it = gst_element_iterate_src_pads(node);
	while (!done) {
		switch (gst_iterator_next(it, (void **)&pad)) {
		case GST_ITERATOR_OK:
			if (gst_pad_is_linked(pad))
				continue;
			name = QString::fromAscii(gst_pad_get_name(pad));
			if (padName == "" || name.contains(padName)) {
				/* We find what we're looking for */
				gst_object_unref (pad);
				done = TRUE;
				break;
			}
			gst_object_unref (pad);
			break;
		case GST_ITERATOR_RESYNC:
			gst_iterator_resync(it);
			break;
		case GST_ITERATOR_ERROR:
			done = TRUE;
			break;
		case GST_ITERATOR_DONE:
			done = TRUE;
			break;
		}
	}
	gst_iterator_free(it);

	return name;
}

int AbstractGstreamerInterface::linkElement2(GstPipeElement *el, int currentPos, int linkNo)
{
	if (linkNo < 0)
		return -1;

	GstPipeElement *prevEl = findPreviousElementToLink(currentPos, linkNo);
	if (!prevEl)
		return -1;

	GstElement *node = gstPipeElements[el->nodeName()];
	GstElement *prev_node = gstPipeElements[prevEl->nodeName()];

	if (prev_node == NULL || node == NULL)
		return -1;

	gst_element_get_request_pad(node, qPrintable(el->getElementPads()[linkNo]));

	/* source has sometimes pad */
	g_signal_connect(prev_node, "pad-added", G_CALLBACK(demux_pad_added), el);
	g_signal_connect(prev_node, "pad-removed", G_CALLBACK(demux_pad_removed), el);

	/*mInfo("%s: link established from %s to %s:\n"
		   "\tlink type\t\t: %s\n"
		   "\twith caps\t\t: %s\n"
		   "\tsrc pad name\t: %s\n"
		   "\tsink pad name\t: %s\n"
		   , __PRETTY_FUNCTION__, node->object.name, prev_node->object.name,
		   linkType == 0 ? "static" : (linkType == 1 ? "sometimes" : "request"),
		   caps1 ? "yes" : "no",
		   qPrintable(srcPadName), qPrintable(destPadName));*/
	return 0;
}

/**
  * link one element in the pipeline to other.
  *
  * \param el Pipe element for linking
  * \param currentPos Position of this element in the pipeline
  * \param caps1 caps for the link, if exits otherwise NULL
  * \param linkNo Link number for this element, can be -1 for single link elements.
  *               This parameter is only meaningful if you only add source elements
  *               using 'addSourceLink' API for this element. Then linkElement
  *               will perform linking for this specific link.
  */
int AbstractGstreamerInterface::linkElement(GstPipeElement *el, int currentPos, GstCaps *caps1, int linkNo)
{
	int linkType = 0;
	int link = 0;
	GstPad *srcPad = NULL;
	GstPad *destPad = NULL;
	QString srcPadName;
	QString destPadName;

	GstPipeElement *prevEl = findPreviousElementToLink(currentPos, linkNo);
	if (!prevEl)
		return -1;

	GstElement *node = gstPipeElements[el->nodeName()];
	GstElement *prev_node = gstPipeElements[prevEl->nodeName()];
	if (prev_node == NULL || node == NULL)
		return -1;

	if (linkNo < 0)
		/* if linkNo is not defined, then pick any pad */
		srcPadName = findPadToLink(prev_node, false, "");
	else
		/* search for a specific link */
		srcPadName = findPadToLink(prev_node, false, el->getSourceElementPads()[linkNo]);

	if (srcPadName.isEmpty()) {
		if (linkNo < 0)
			srcPad = gst_element_get_request_pad(prev_node, "src%d");
		else
			srcPad = gst_element_get_request_pad(prev_node, qPrintable(el->getSourceElementPads()[linkNo]));
		if (srcPad)
			linkType = 2;
		else {
			mInfo("%s: cannot find src pad of %s, I think it has sometimes pad", __PRETTY_FUNCTION__, prev_node->object.name);
			linkType = 1;
		}
	}

	if (linkNo < 0)
		destPadName = findPadToLink(node, true, "");
	else
		destPadName = findPadToLink(node, true, el->getElementPads()[linkNo]);
	if (destPadName.isEmpty()) {
		if (linkNo < 0)
			destPad = gst_element_get_request_pad(node, "src%d");
		else
			destPad = gst_element_get_request_pad(node, qPrintable(el->getElementPads()[linkNo]));
		link = 2;
	} else
		link = 1;

	switch (linkType) {
	case 0: //static src pads
		if (link == 1) {
			if (caps1) {
				if (gst_element_link_pads_filtered(prev_node, qPrintable(srcPadName),
												   node, qPrintable(destPadName), caps1) == FALSE) {
					mDebug("%s: unable to link %s to %s with caps", __PRETTY_FUNCTION__,
						   node->object.name, prev_node->object.name);
					return -1;
				}
			} else {
				if (gst_element_link_pads(prev_node, qPrintable(srcPadName), node, qPrintable(destPadName)) == FALSE) {
					mDebug("%s: unable to link %s to %s", __PRETTY_FUNCTION__, node->object.name, prev_node->object.name);
					return -1;
				}
			}
		} else {
			srcPad = gst_element_get_static_pad(prev_node, qPrintable(srcPadName));
			gst_pad_link(srcPad, destPad);
		}
		break;
	case 1: //sometimes src pads
		if (link == 1)
			destPad = gst_element_get_static_pad(node, qPrintable(destPadName));
		g_signal_connect(prev_node, "pad-added", G_CALLBACK(demux_pad_added), el);
		g_signal_connect(prev_node, "pad-removed", G_CALLBACK(demux_pad_removed), el);

		break;
	case 2: // request src pads
		if (link == 1)
			destPad = gst_element_get_static_pad(node, qPrintable(destPadName));
		gst_pad_link(srcPad, destPad);
		break;
	}

	return 0;
}

#define setBoolParameters(node, it) \
	while (it.hasNext()) { \
	it.next(); \
	g_object_set(G_OBJECT(node), qPrintable(it.key()), (gboolean)it.value(), NULL); \
	}
#define setIntParameters(node, it) \
	while (it.hasNext()) { \
	it.next(); \
	g_object_set(G_OBJECT(node), qPrintable(it.key()), it.value(), NULL); \
	}
#define setStrParameters(node, it) \
	while (it.hasNext()) { \
	it.next(); \
	g_object_set(G_OBJECT(node), qPrintable(it.key()), qPrintable(it.value()), NULL); \
	}

int AbstractGstreamerInterface::setElementParameters(GstPipeElement *el, GstElement *node)
{
	QMapIterator<QString, bool> it1(el->boolValues);
	QMapIterator<QString, int> it2(el->intValues);
	QMapIterator<QString, QString> it3(el->strValues);
	QMapIterator<QString, unsigned long long> it4(el->longLongValues);
	QMapIterator<QString, GValue *> it5(el->gValues);
	while (it1.hasNext()) {
		it1.next();
		g_object_set(G_OBJECT(node), qPrintable(it1.key()), (gboolean)it1.value(), NULL);
	}
	while (it2.hasNext()) {
		it2.next();
		g_object_set(G_OBJECT(node), qPrintable(it2.key()), it2.value(), NULL);
	}
	while (it3.hasNext()) {
		it3.next();
		g_object_set(G_OBJECT(node), qPrintable(it3.key()), qPrintable(it3.value()), NULL);
	}
	while (it4.hasNext()) {
		it4.next();
		g_object_set(G_OBJECT(node), qPrintable(it4.key()), it4.value(), NULL);
	}
	while (it5.hasNext()) {
		it5.next();
		g_object_set_property(G_OBJECT(node), qPrintable(it5.key()), it5.value());
	}

	return 0;
}

GstCaps * AbstractGstreamerInterface::createCaps(GstPipeElement *el)
{
	GstCaps *caps1 = NULL;

	caps1 = gst_caps_new_simple (qPrintable(el->gstElementName()), NULL);
	QMapIterator<QString, bool> it1(el->boolValues);
	QMapIterator<QString, int> it2(el->intValues);
	QMapIterator<QString, QString> it3(el->strValues);
	QMapIterator<QString, unsigned long long> it4(el->longLongValues);
	while (it1.hasNext()) {
		it1.next();
		gst_caps_set_simple(caps1, qPrintable(it1.key()), G_TYPE_BOOLEAN, (gboolean)it1.value(), NULL);
	}
	while (it2.hasNext()) {
		it2.next();
		gst_caps_set_simple(caps1, qPrintable(it2.key()), G_TYPE_INT, it2.value(), NULL);
	}
	while (it3.hasNext()) {
		it3.next();
		gst_caps_set_simple(caps1, qPrintable(it3.key()), G_TYPE_STRING, qPrintable(it3.value()), NULL);
	}
	while (it4.hasNext()) {
		it4.next();
		gst_caps_set_simple(caps1, qPrintable(it4.key()), G_TYPE_LONG, it4.value(), NULL);
	}
	return caps1;
}

int AbstractGstreamerInterface::createElements()
{
	GstCaps *caps1 = NULL;
	/* Create pipeline elements */
	for (int i = 0; i < pipeElements.size(); ++i) {
		GstPipeElement *el = pipeElements.at(i);
		mDebug("new element %s", qPrintable(el->elementName));
		if (!el->isCapsFilter()) {
			GstElement *node = gst_element_factory_make(qPrintable(el->gstElementName()), qPrintable(el->nodeName()));
			if (!node) {
				qWarning("unable to create %s element", qPrintable(el->gstElementName()));
				goto err;
			}
			gstPipeElements.insert(el->nodeName(), node);
			el->setGstElement(node);

			/* set element parameters */
			if (setElementParameters(el, node))
				goto err;

			/* must add elements to pipeline before linking them */
			gst_bin_add (GST_BIN(pipeline), node);
		}
	}

	/* All elements created, let's link alltogether */
	for (int i = 0; i < pipeElements.size(); ++i) {
		GstPipeElement *el = pipeElements.at(i);
		if (i == 0 || el->linkThisElement() == false) {
			mDebug("%s first element in the pipeline, skipping linking", qPrintable(el->nodeName()));
			continue;
		}
		if (el->isCapsFilter()) {
			caps1 = createCaps(el);
			if (caps1 == NULL)
				goto err;
		} else {
			/* do linkage */
			QStringList sourceElements = el->getSourceElementNames();
			if (sourceElements.size()) {
				for (int j = 0; j < sourceElements.size(); ++j) {
					if (el->srcPadTypes[j] == GstPipeElement::PAD_ALWAYS) {
						if (linkElement(el, i, caps1, j))
							goto err;
					} else if (el->srcPadTypes[j] == GstPipeElement::PAD_SOMETIMES
							   && el->destPadTypes[j] == GstPipeElement::PAD_ALWAYS) {
						if (linkElement(el, i, caps1, j))
							goto err;
					} else if (el->srcPadTypes[j] == GstPipeElement::PAD_SOMETIMES
							   && el->destPadTypes[j] == GstPipeElement::PAD_REQUEST) {
						if (linkElement2(el, i, j))
							goto err;
					}
				}
			} else if (linkElement(el, i, caps1))
				goto err;
			caps1 = NULL;
		}
	}

	return 0;

err:

	return -1;
}

int AbstractGstreamerInterface::createPipeline()
{
	GstBus *bus;

	if (pipeline) {
		mDebug("pipeline is already created!");
		return -1;
	}

	/* Create pipeline */
	pipeline = gst_pipeline_new(qPrintable(pipelineName));
	if (!pipeline) {
		qWarning("unable to create video pipeline element");
		goto err;
	}

	if (createElements())
		goto err;

	/* Monitor bus messages for error handling */
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	gst_bus_add_watch (bus, handleBusMessage, this);
	gst_object_unref (bus);
	return 0;

err:
	if (pipeline)
		gst_object_unref(GST_OBJECT(pipeline));
	return -1;
}

int AbstractGstreamerInterface::destroyPipeline()
{
	if (!pipeline) {
		mDebug("pipeline is already destroyed!");
		return -1;
	}
#if 0
	gst_object_unref(GST_OBJECT (pipeline));
	if (GST_OBJECT_REFCOUNT(pipeline))
		qWarning("pipeline is not destroyed. Seems like some other object is holding refs to it.");
	else
		pipeline = NULL;
#else
	/*
	 * Normally we would unref pipeline let glib handle all the rest for us.
	 * However, doing that way we sometime may hang indefinitely waiting in
	 * g_object_unref. I don't know if there is bug entry for this in somewhere
	 * but this is(was???) the case for us. So we delete object our selves.
	 */
	g_type_free_instance((GTypeInstance*)pipeline);
	pipeline = NULL;
#endif

	return 0;
}

int AbstractGstreamerInterface::installPadProbes()
{
	if (padProbesInstalled)
		return -1;

	for (int i = 0; i < pipeElements.size(); ++i) {
		GstPipeElement *el = pipeElements[i];
		if (el->isCapsFilter() || !el->watchElement)
			continue;
		GstElement *gEl = gstPipeElements[el->nodeName()];
		GstPad *pad = gst_element_get_static_pad(gEl, "sink");
		if (!pad)
			pad = gst_element_get_static_pad(gEl, "src");
		if (pad)
			bufferProbeIDs.insert(gst_pad_add_buffer_probe(pad, G_CALLBACK(padWatcher), el), pad);
		else
			mDebug("cannot install pad probe for element %s", gEl->object.name);
	}
	padProbesInstalled = true;

	return 0;
}

int AbstractGstreamerInterface::installFlowPadProbe(QString pattern)
{
	for (int i = 0; i < pipeElements.size(); ++i) {
		GstPipeElement *el = pipeElements[i];
		if (el->isCapsFilter() || !el->nodeName().contains(pattern, Qt::CaseInsensitive))
			continue;
		GstElement *gEl = gstPipeElements[el->nodeName()];
		GstPad *pad;
		pad = gst_element_get_static_pad(gEl, "sink");
		if (pad)
			flowBufferProbeIDs.insert(gst_pad_add_buffer_probe(pad, G_CALLBACK(padWatcher), el), pad);
		else
			mDebug("cannot install pad probe for element %s", gEl->object.name);
	}

	return 0;
}

int AbstractGstreamerInterface::removeFlowPadProbes()
{
	for (int i = 0; i < flowBufferProbeIDs.keys().size(); ++i) {
		unsigned int id = flowBufferProbeIDs.keys()[i];
		gst_pad_remove_data_probe(flowBufferProbeIDs[id], id);
	}

	return 0;
}

int AbstractGstreamerInterface::removePadProbes()
{
	if (!padProbesInstalled)
		return -1;

	for (int i = 0; i < bufferProbeIDs.keys().size(); ++i) {
		unsigned int id = bufferProbeIDs.keys()[i];
		mDebug("removing installed pad probe %d", id);
		gst_pad_remove_data_probe(bufferProbeIDs[id], id);
	}
	padProbesInstalled = false;

	return 0;
}

int AbstractGstreamerInterface::checkPipelineStatus()
{
	GstPipeElement *el;
	int res = 0;
	if (pipeline == NULL)
		return 0;

	/* We use pad probes to watch elements. */
	installPadProbes();

	for (int i = 0; i < pipeElements.size(); ++i) {
		el = pipeElements[i];
		if (el->watchElement == false || el->isCapsFilter())
			continue;
		if (el->bufferCount == el->lastBufferCount) {
			res |= 1 << i;
			mDebug("%s buffer count is not increasing", qPrintable(el->nodeName()));
		}
		el->lastBufferCount = el->bufferCount;
	}

	return res;
}

void AbstractGstreamerInterface::pipelineStateChanged(GstState state)
{
	mDebug("pipeline state change %d", state);
	if (state == GST_STATE_PLAYING) {
		pipelineRunning = true;
		pipelinePaused = false;
	}
	if (state == GST_STATE_NULL) {
		pipelineRunning = false;
	}
	gstPipelineState = state;
}

GstElement * AbstractGstreamerInterface::getGstPipeElement(QString name)
{
	if (!gstPipeElements.contains(name))
		return NULL;

	return gstPipeElements[name];
}

unsigned long long AbstractGstreamerInterface::getPlaybackPosition()
{
	if (!this->isRunning())
		return 0;
	GstFormat fmt = GST_FORMAT_TIME;
	if (!positionQuery)
		positionQuery = gst_query_new_position(fmt);
	gint64 pos;
	gboolean ret;
	/*
	 * If we query pipeline for playback position, it will
	 * probably go through demux element and that will
	 * make a random query on one of its pads. However, some
	 * audio elements do not these queries gracefully. To avoid
	 * this we select video decoder element and ask query to it.
	 * Note that decoder element shouldn't need to support
	 * querying, this is only for using video pad of demux element.
	 * If there is no element named videoDecoder then we will
	 * query pipeline as a fallback mechanism.
	 *
	 */
	GstElement *dec = getGstPipeElement("videoDecoder");
	if (!dec)
		dec = pipeline;

	ret = gst_element_query(dec, positionQuery);
	if (ret) {
		gst_query_parse_position(positionQuery, &fmt, &pos);
		return pos;
	}

	return 0;
}

unsigned long long AbstractGstreamerInterface::getPlaybackDuration()
{
	if (playbackDuration)
		return playbackDuration;

	if (!this->isRunning())
		return 0;

	GstFormat fmt = GST_FORMAT_TIME;

	/*  see the note at getPlaybackPosition */
	GstElement *dec = getGstPipeElement("videoDecoder");
	if (!dec)
		dec = pipeline;

	if (gst_element_query_duration(dec, &fmt, &playbackDuration))
		return playbackDuration;

	playbackDuration = 0;
	return 0;
}

void AbstractGstreamerInterface::seek(unsigned long long nanoSecs)
{
	mDebug("seeking %lld nanosecs", nanoSecs);
	seekToTime(getPlaybackPosition() + nanoSecs);
}

void AbstractGstreamerInterface::setPlaybackSpeed(gdouble rate)
{
	int flags = GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SKIP;
	if (!gst_element_seek(pipeline, rate, GST_FORMAT_TIME, (GstSeekFlags)flags,
						  GST_SEEK_TYPE_SET, getPlaybackPosition(),
						  GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
		mDebug("Seek failed!\n");
	}
}

void AbstractGstreamerInterface::seekToTime(unsigned long long nanoSecs)
{
	int flags = GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SKIP;
	if (!gst_element_seek(pipeline, 1.0, GST_FORMAT_TIME, (GstSeekFlags)flags,
						  GST_SEEK_TYPE_SET, nanoSecs,
						  GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
		mDebug("Seek failed!\n");
	}
}

void AbstractGstreamerInterface::sync()
{
	if (startSyncWait && sSync.available() == 0)
		sSync.release(1);
}
