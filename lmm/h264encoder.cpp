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

#define dmaiCapture() 1

H264Encoder::H264Encoder(QObject *parent) :
	BaseLmmElement(parent)
{
	if (dmaiCapture() == 0)
		input = new DM365CameraInput;
	else
		input = new DM365DmaiCapture;
	elements << input;

	encoder = new DmaiEncoder;
	elements << encoder;
#if 0
	DM365VideoOutput *dm365Output = new DM365VideoOutput;
	dm365Output->setVideoOutput(DM365VideoOutput::COMPOSITE);
	output = dm365Output;
#else
	output =  new FileOutput;
#endif
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
	RawBuffer *buf = input->nextBuffer();
	if (buf) {
#if 1
		encoder->addBuffer(buf);
		if (!encoder->encodeNext()) {
			buf = encoder->nextBuffer();
			if (buf)
				output->addBuffer(buf);
			output->output();
			//buf = output->nextBuffer();
			//if (buf)
			//input->addBuffer(buf);
		}
#else
		output->addBuffer(buf);
		output->output();
	if (dmaiCapture()) {
		buf = output->nextBuffer();
		if (buf)
			input->addBuffer(buf);
	} else
		delete buf;
#endif
	}
	timer->start(1);
}
