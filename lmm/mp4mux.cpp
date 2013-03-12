#define __STDC_CONSTANT_MACROS

#include "mp4mux.h"

#include "debug.h"

extern "C" {
	#include "libavformat/avformat.h"
}

Mp4Mux::Mp4Mux(QObject *parent) :
	BaseLmmMux(parent)
{
	sourceUrlName = "test.mp4";
	fmt = av_guess_format("mp4", NULL, NULL);
}
