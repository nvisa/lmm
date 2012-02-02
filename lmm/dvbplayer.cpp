#include "dvbplayer.h"
#include "rawbuffer.h"
#include "v4l2input.h"
#include "fileoutput.h"
#include "maddecoder.h"
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
	mainSource = v4l2;
	elements << demux;
	elements << v4l2;

	videoDecoder = new DmaiDecoder(DmaiDecoder::MPEG2);
	videoOutput = new FbOutput;
	elements << videoDecoder;
	elements << videoOutput;

	audioDecoder = new MadDecoder;
	audioOutput = new AlsaOutput;
	alsaControl = ((AlsaOutput *)audioOutput)->alsaControl();
	elements << audioDecoder;
	elements << audioOutput;

	live = true;
}

int DvbPlayer::tuneToChannel(QString channelUrl)
{
	return demux->setSource(channelUrl);
}

DvbPlayer::~DvbPlayer()
{
}
