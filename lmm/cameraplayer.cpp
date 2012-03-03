#include "cameraplayer.h"
#include "fboutput.h"
#include "v4l2input.h"
#include "rawbuffer.h"

#include <emdesk/debug.h>

#include <QTimer>

CameraPlayer::CameraPlayer(QObject *parent) :
	BaseLmmPlayer(parent)
{
	input = new V4l2Input;
	elements << input;

	videoOutput = new FbOutput;
	videoOutput->syncOnClock(false);
	elements << videoOutput;
}

int CameraPlayer::decodeLoop()
{
	RawBuffer *buf = input->nextBuffer();
	if (buf) {
		videoOutput->addBuffer(buf);
		videoOutput->output();
	}
	timer->start(20);
	return 0;
}
