#include "avidecoder.h"
#include "avidemux.h"
#include "maddecoder.h"
#include "rawbuffer.h"
#include "alsaoutput.h"
#include "fboutput.h"
#include "dmaidecoder.h"
#include "streamtime.h"
#include "emdesk/debug.h"
#include "emdesk/hardwareoperations.h"
#include "alsa/alsa.h"

#include <QTime>
#include <QTimer>
#include <QThread>

#include <errno.h>

AviPlayer::AviPlayer(QObject *parent) :
	BaseLmmPlayer(parent)
{
	audioDecoder = new MadDecoder;
	videoDecoder = new DmaiDecoder(DmaiDecoder::MPEG4);
	audioOutput = new AlsaOutput;
	videoOutput = new FbOutput;
	demux = new AviDemux;
	elements << demux;
	elements << videoDecoder;
	elements << videoOutput;
	elements << audioDecoder;
	elements << audioOutput;
	alsaControl = ((AlsaOutput *)audioOutput)->alsaControl();
}

AviPlayer::~AviPlayer()
{

}
