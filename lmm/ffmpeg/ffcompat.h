#ifndef FFCOMPAT_H
#define FFCOMPAT_H

extern "C" {
	#include "libavformat/avformat.h"
	#include "libavformat/avio.h"
}

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(52, 102, 0)
#define avio_open url_fopen
#define avio_close url_fclose
static void avformat_free_context(AVFormatContext *c)
{
	delete c;
}

#endif
#endif // FFCOMPAT_H
