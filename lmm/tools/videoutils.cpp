#include "videoutils.h"

#include <linux/videodev2.h>

VideoUtils::VideoUtils()
{
}

int VideoUtils::getLineLength(int pixFmt, int width)
{
	if (pixFmt == V4L2_PIX_FMT_YUYV)
		return width * 2;
	if (pixFmt == V4L2_PIX_FMT_UYVY)
		return width * 2;
	if (pixFmt == V4L2_PIX_FMT_NV12)
		return width;
	if (pixFmt == V4L2_PIX_FMT_SBGGR16)
		return width * 2;
	if (pixFmt == V4L2_PIX_FMT_SBGGR8)
		return width;
	if (pixFmt == V4L2_PIX_FMT_SGRBG10)
		return width * 2;
	if (pixFmt == V4L2_PIX_FMT_YUV420)
		return width;
	return 0;
}

int VideoUtils::getFrameSize(int pixFmt, int width, int height)
{
	if (pixFmt == V4L2_PIX_FMT_NV12 || pixFmt == V4L2_PIX_FMT_YUV420)
		return width * height * 3 / 2;
	return getLineLength(pixFmt, width) * height;
}
