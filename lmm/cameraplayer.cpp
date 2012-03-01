#include "cameraplayer.h"
#include "fboutput.h"
#include "v4l2input.h"

CameraPlayer::CameraPlayer(QObject *parent) :
	BaseLmmPlayer(parent)
{
	input = new V4l2Input;
	elements << input;

	videoOutput = new FbOutput;
	videoOutput->syncOnClock(false);
	elements << videoOutput;
}
