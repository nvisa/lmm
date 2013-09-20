#define __STDC_CONSTANT_MACROS

#include "mp4mux.h"

#include "debug.h"

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h> /* for URLContext on x86 */
	#include "ffcompat.h"
}

Mp4Mux::Mp4Mux(QObject *parent) :
	BaseLmmMux(parent)
{
	sourceUrlName = "lmmmuxo";
	fmt = av_guess_format("mp4", NULL, NULL);
}

int64_t Mp4Mux::seekUrl(int64_t pos, int whence)
{
	if (pos == currUrlPos)
		return pos;
	return -EINVAL;
}

QString Mp4Mux::mimeType()
{
	return "video/quicktime";
}
