#include "neoncolorspace.h"
#include "libyuv.h"
#include "debug.h"
#include "tools/videoutils.h"

#include <linux/videodev2.h>

static int UYVYToI420(const uint8* src_uyvy, int src_stride_uyvy,
					  uint8* dst_y, int dst_stride_y,
					  uint8* dst_u, int dst_stride_u,
					  uint8* dst_v, int dst_stride_v,
					  int width, int height) {
	int y;
	/* NOTE: input should be 16-byte aligned */
	void (*UYVYToUVRow)(const uint8* src_uyvy, int src_stride_uyvy,
						uint8* dst_u, uint8* dst_v, int pix) = UYVYToUVRow_NEON;
	void (*UYVYToYRow)(const uint8* src_uyvy,
					   uint8* dst_y, int pix) = UYVYToYRow_NEON;
	// Negative height means invert the image.
	if (height < 0) {
		height = -height;
		src_uyvy = src_uyvy + (height - 1) * src_stride_uyvy;
		src_stride_uyvy = -src_stride_uyvy;
	}

	for (y = 0; y < height - 1; y += 2) {
		UYVYToUVRow(src_uyvy, src_stride_uyvy, dst_u, dst_v, width);
		UYVYToYRow(src_uyvy, dst_y, width);
		UYVYToYRow(src_uyvy + src_stride_uyvy, dst_y + dst_stride_y, width);
		src_uyvy += src_stride_uyvy * 2;
		dst_y += dst_stride_y * 2;
		dst_u += dst_stride_u;
		dst_v += dst_stride_v;
	}
	if (height & 1) {
		UYVYToUVRow(src_uyvy, 0, dst_u, dst_v, width);
		UYVYToYRow(src_uyvy, dst_y, width);
	}
	return 0;
}

static int YUY2ToI420(const uint8* src_yuy2, int src_stride_yuy2,
					  uint8* dst_y, int dst_stride_y,
					  uint8* dst_u, int dst_stride_u,
					  uint8* dst_v, int dst_stride_v,
					  int width, int height) {
	int y;
	void (*YUY2ToUVRow)(const uint8* src_yuy2, int src_stride_yuy2,
						uint8* dst_u, uint8* dst_v, int pix) = YUY2ToUVRow_NEON;
	void (*YUY2ToYRow)(const uint8* src_yuy2,
					   uint8* dst_y, int pix) = YUY2ToYRow_NEON;
	// Negative height means invert the image.
	if (height < 0) {
		height = -height;
		src_yuy2 = src_yuy2 + (height - 1) * src_stride_yuy2;
		src_stride_yuy2 = -src_stride_yuy2;
	}

	for (y = 0; y < height - 1; y += 2) {
		YUY2ToUVRow(src_yuy2, src_stride_yuy2, dst_u, dst_v, width);
		YUY2ToYRow(src_yuy2, dst_y, width);
		YUY2ToYRow(src_yuy2 + src_stride_yuy2, dst_y + dst_stride_y, width);
		src_yuy2 += src_stride_yuy2 * 2;
		dst_y += dst_stride_y * 2;
		dst_u += dst_stride_u;
		dst_v += dst_stride_v;
	}
	if (height & 1) {
		YUY2ToUVRow(src_yuy2, 0, dst_u, dst_v, width);
		YUY2ToYRow(src_yuy2, dst_y, width);
	}
	return 0;
}

static void CopyPlane(const uint8* src_y, int src_stride_y,
					  uint8* dst_y, int dst_stride_y,
					  int width, int height) {
	int y;
	void (*CopyRow)(const uint8* src, uint8* dst, int width) = CopyRow_NEON;
	// Coalesce rows.
	if (src_stride_y == width &&
			dst_stride_y == width) {
		width *= height;
		height = 1;
		src_stride_y = dst_stride_y = 0;
	}

	// Copy plane
	for (y = 0; y < height; ++y) {
		CopyRow(src_y, dst_y, width);
		src_y += src_stride_y;
		dst_y += dst_stride_y;
	}
}

NeonColorSpace::NeonColorSpace(QObject *parent) :
	BaseLmmElement(parent)
{
}

int NeonColorSpace::processBuffer(const RawBuffer &buf)
{
	int width = buf.constPars()->videoWidth;
	int height = buf.constPars()->videoHeight;
	int pixFmt = buf.constPars()->v4l2PixelFormat;
	int outPixFmt = V4L2_PIX_FMT_YUV420;
	int size = VideoUtils::getFrameSize(outPixFmt, width, height);
	uchar *data = (uchar *)buf.constData();
	RawBuffer outb("video/x-raw-yuv", size);
	uchar *dst = (uchar *)outb.data();
	uchar *dstu = dst + width * height;
	uchar *dstv = dstu + width * height / 4;
	if (pixFmt == V4L2_PIX_FMT_UYVY)
		UYVYToI420(data, width * 2, dst, width, dstu, width / 2, dstv, width / 2, width, height);
	else if (pixFmt == V4L2_PIX_FMT_YUYV)
		YUY2ToI420(data, width * 2, dst, width, dstu, width / 2, dstv, width / 2, width, height);

	outb.pars()->videoWidth = width;
	outb.pars()->videoHeight = height;
	outb.pars()->v4l2PixelFormat = outPixFmt;

	return newOutputBuffer(0, outb);
}
