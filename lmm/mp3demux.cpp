#define __STDC_CONSTANT_MACROS

#include "mp3demux.h"

extern "C" {
	#include "libavformat/avformat.h"
}

Mp3Demux::Mp3Demux(QObject *parent) :
	BaseLmmDemux(parent)
{
}

QString Mp3Demux::mimeType()
{
	return "audio/mpeg";
}

int Mp3Demux::findStreamInfo()
{
	return BaseLmmDemux::findStreamInfo();
}
