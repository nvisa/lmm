#include "dvbplayer.h"
#include "rawbuffer.h"
#include "v4l2input.h"
#include "fileoutput.h"
#include "mad.h"
#include "alsaoutput.h"
#include "fboutput.h"
#include "dmaidecoder.h"
#include "circularbuffer.h"
#include "baselmmdemux.h"
#include "mpegtsdemux.h"
#include "emdesk/debug.h"

DvbPlayer::DvbPlayer(QObject *parent) :
	BaseLmmPlayer(parent)
{
	demux = new MpegTsDemux;
	v4l2 = new V4l2Input;
	audioDecoder = new Mad;
	audioOutput = new AlsaOutput;
	videoDecoder = new DmaiDecoder(DmaiDecoder::MPEG2);
	videoOutput = new FbOutput;

	elements << demux;
	elements << v4l2;
	elements << audioDecoder;
	elements << audioOutput;
	elements << videoDecoder;
	elements << videoOutput;

	//videoOutput->syncOnClock(false);
	//audioOutput->syncOnClock(false);
	live = true;
}

DvbPlayer::~DvbPlayer()
{
}

int DvbPlayer::play()
{
	demux->setSource(v4l2->getCircularBuffer());
	return BaseLmmPlayer::play();
}

int DvbPlayer::stop()
{
	return BaseLmmPlayer::stop();
}
