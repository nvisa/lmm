#include "mp3player.h"
#include "mp3demux.h"
#include "maddecoder.h"
#include "alsa/alsaoutput.h"

Mp3Player::Mp3Player(QObject *parent) :
	BaseLmmPlayer(parent)
{
	demux = new Mp3Demux;
	elements << demux;

	audioDecoder = new MadDecoder;
	audioOutput = new AlsaOutput;
	elements << audioDecoder;
	elements << audioOutput;
	alsaControl = ((AlsaOutput *)audioOutput)->alsaControl();
}
