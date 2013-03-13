#include "dvbplayer.h"
#include "rawbuffer.h"
#include "v4l2input.h"
#include "fileoutput.h"
#include "maddecoder.h"
#include "alsa/alsaoutput.h"
#include "dm6446/dm6446fboutput.h"
#include "dmai/dmaidecoder.h"
#include "circularbuffer.h"
#include "baselmmdemux.h"
#include "mpegtsdemux.h"
#include "debug.h"

DvbPlayer::DvbPlayer(QObject *parent) :
	BaseLmmPlayer(parent)
{
	demux = new MpegTsDemux;
	elements << demux;

	V4l2Input *v4l2 = new V4l2Input;
	mainSource = v4l2;
	elements << v4l2;

	videoDecoder = new DmaiDecoder(DmaiDecoder::MPEG2);
	videoOutput = new DM6446FbOutput;
	elements << videoDecoder;
	elements << videoOutput;

	audioDecoder = new MadDecoder;
	audioOutput = new AlsaOutput;
#ifdef CONFIG_ALSA
	alsaControl = ((AlsaOutput *)audioOutput)->alsaControl();
#endif
	elements << audioDecoder;
	elements << audioOutput;

	live = true;
}

int DvbPlayer::tuneToChannel(QString channelUrl)
{
	videoOutput->syncOnClock(true);
	audioOutput->syncOnClock(true);
	return demux->setSource(channelUrl);
}

DvbPlayer::~DvbPlayer()
{
}

int DvbPlayer::decodeLoop()
{
	if (videoDecoder->getInputBufferCount() > 100) {
		videoDecoder->flush();
		videoOutput->flush();
		videoOutput->syncOnClock(false);
		audioOutput->syncOnClock(false);
	}
	return BaseLmmPlayer::decodeLoop();
}
