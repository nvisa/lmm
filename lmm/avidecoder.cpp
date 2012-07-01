#include "avidecoder.h"
#include "avidemux.h"
#include "maddecoder.h"
#include "rawbuffer.h"
#include "alsaoutput.h"
#include "blec32fboutput.h"
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
	demux = new AviDemux;
	elements << demux;

	audioDecoder = new MadDecoder;
	audioOutput = new AlsaOutput;
	elements << audioDecoder;
	elements << audioOutput;
#ifdef CONFIG_ALSA
	alsaControl = ((AlsaOutput *)audioOutput)->alsaControl();
#endif

	videoDecoder = new DmaiDecoder(DmaiDecoder::MPEG4);
	videoOutput = new Blec32FbOutput;
	elements << videoDecoder;
	elements << videoOutput;
}

AviPlayer::~AviPlayer()
{

}

int AviPlayer::decodeLoop()
{
	int err = BaseLmmPlayer::decodeLoop();
	if (err)
		return err;

	err = videoOutput->getInputBufferCount();
	if (err == 2) {
		mDebug("buffer hold detected, flushing video output");
		videoOutput->flush();
	}
	return 0;
}
