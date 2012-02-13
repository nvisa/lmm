#include "mp3demux.h"

extern "C" {
	#include "libavformat/avformat.h"
}

Mp3Demux::Mp3Demux(QObject *parent) :
	BaseLmmDemux(parent)
{
}

int Mp3Demux::findStreamInfo()
{
	return BaseLmmDemux::findStreamInfo();
}
