#define __STDC_CONSTANT_MACROS

#include "avimux.h"

extern "C" {
	#include "libavformat/avformat.h"
}

AviMux::AviMux(QObject *parent) :
	BaseLmmMux(parent)
{
	sourceUrlName = "lmmmuxo";
	fmt = av_guess_format("avi", NULL, NULL);
}

QString AviMux::mimeType()
{
	return "video/x-msvideo";
}
