#include "h264encoder.h"
#include "dm365camerainput.h"
#include "dm365dmaicapture.h"

#include "fileoutput.h"
#include "dmaiencoder.h"
#include "rawbuffer.h"
#include "fboutput.h"
#include "dm365videooutput.h"

#include <emdesk/debug.h>

#include <QTimer>

#include <errno.h>

H264Encoder::H264Encoder(QObject *parent) :
	BaseLmmElement(parent)
{
	input = new DM365CameraInput;
	elements << input;
	encoder = new DmaiEncoder;
	elements << encoder;
	//output = new FileOutput;
	DM365VideoOutput *dm365Output = new DM365VideoOutput;
	dm365Output->setVideoOutput(DM365VideoOutput::COMPOSITE);
	output = dm365Output;
	output->syncOnClock(false);

	elements << output;

	timer = new QTimer(this);
	timer->setSingleShot(true);
	connect(timer, SIGNAL(timeout()), this, SLOT(encodeLoop()));
}

int H264Encoder::start()
{
	foreach (BaseLmmElement *el, elements) {
		mInfo("starting element %s", el->metaObject()->className());
		el->flush();
		int err = el->start();
		if (err)
			return err;
	}
	timer->start(10);
	return BaseLmmElement::start();
}

int H264Encoder::stop()
{
	foreach (BaseLmmElement *el, elements) {
		el->setStreamTime(NULL);
		el->stop();
	}
	return BaseLmmElement::stop();
}

void H264Encoder::encodeLoop()
{
	//mDebug("encoding");
	RawBuffer *buf = input->nextBuffer();
	if (buf) {
#if 0
		encoder->addBuffer(buf);
		if (!encoder->encodeNext()) {
			buf = encoder->nextBuffer();
			if (buf)
				output->addBuffer(buf);
			output->output();
			buf = output->nextBuffer();
			if (buf)
				input->addBuffer(buf);
#else
		output->addBuffer(buf);
		output->output();
		buf = output->nextBuffer();
		if (buf)
			input->addBuffer(buf);
#endif
	}
	timer->start(10);
}
