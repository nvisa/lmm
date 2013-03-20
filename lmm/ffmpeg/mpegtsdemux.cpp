#include "mpegtsdemux.h"
#include "circularbuffer.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "debug.h"

MpegTsDemux::MpegTsDemux(QObject *parent) :
	BaseLmmDemux(parent)
{
	context = NULL;
	libavAnalayzeDuration = 500000;
}

int MpegTsDemux::start()
{
	return BaseLmmDemux::start();
}

int MpegTsDemux::stop()
{
	return BaseLmmDemux::stop();
}

int MpegTsDemux::seekTo(qint64)
{
	return -1;
}

QString MpegTsDemux::mimeType()
{
	return "video/mpegts";
}
