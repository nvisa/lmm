#include "avidecoder.h"
#include "avidemux.h"
#include "mad.h"
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
	audioDecoder = new Mad;
	videoDecoder = new DmaiDecoder;
	audioOutput = new AlsaOutput;
	videoOutput = new FbOutput;
	demux = new AviDemux;
	elements << demux;
	elements << videoDecoder;
	elements << videoOutput;
	elements << audioDecoder;
	elements << audioOutput;
}

AviPlayer::~AviPlayer()
{

}

int AviPlayer::startDecoding()
{
	return 0;
}

void AviPlayer::stopDecoding()
{
}
