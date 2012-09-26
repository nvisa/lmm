#include "videoutils.h"

#include <linux/videodev2.h>

VideoUtils::VideoUtils()
{
}

int VideoUtils::getLineLength(int pixFmt, int width)
{
	if (pixFmt == V4L2_PIX_FMT_UYVY)
		return width * 2;
	if (pixFmt == V4L2_PIX_FMT_NV12)
		return width;
	return 0;
}
