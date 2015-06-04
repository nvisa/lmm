#include "neonvideoscaler.h"
#include "tools/videoutils.h"
#include "libyuv.h"
#include "debug.h"

#include <linux/videodev2.h>

typedef uchar uint8 ;
extern "C" {

}

static void ScalePlaneDown2(int dst_width, int dst_height,
							int src_stride, int dst_stride,
							const uint8* src_ptr, uint8* dst_ptr)
{
	int y;
	void (*ScaleRowDown2)(const uint8* src_ptr, ptrdiff_t src_stride,
						  uint8* dst_ptr, int dst_width) = ScaleRowDown2_NEON;
	int row_stride = src_stride << 1;
	int filtering = 0;
	if (!filtering) {
		src_ptr += src_stride;  // Point to odd rows.
		src_stride = 0;
	}
	// TODO(fbarchard): Loop through source height to allow odd height.
	for (y = 0; y < dst_height; ++y) {
		ScaleRowDown2(src_ptr, src_stride, dst_ptr, dst_width);
		src_ptr += row_stride;
		dst_ptr += dst_stride;
	}
}

NeonVideoScaler::NeonVideoScaler(QObject *parent) :
	BaseLmmElement(parent)
{
}

int NeonVideoScaler::processBuffer(const RawBuffer &buf)
{
	int width = buf.constPars()->videoWidth;
	int height = buf.constPars()->videoHeight;
	int pixFmt = buf.constPars()->v4l2PixelFormat;
	int scaledWidth = width / 2;
	int scaledHeight = height / 2;
	int outSize = VideoUtils::getFrameSize(pixFmt, scaledWidth, scaledHeight);

	const uchar *dst = (const uchar *)buf.constData();
	const uchar *dstu = dst + width * height;
	const uchar *dstv = dstu + width * height / 4;

	RawBuffer outb(buf.getMimeType(), outSize);
	uchar *sdst = (uchar *)outb.data();
	uchar *sdstu = sdst + scaledWidth * scaledHeight;
	uchar *sdstv = sdstu + scaledWidth * scaledHeight / 4;
	ScalePlaneDown2(scaledWidth, scaledHeight, width, scaledWidth, dst, sdst);
	ScalePlaneDown2(scaledWidth / 2, scaledHeight / 2, width / 2, scaledWidth / 2, dstu, sdstu);
	ScalePlaneDown2(scaledWidth / 2, scaledHeight / 2, width / 2, scaledWidth / 2, dstv, sdstv);

	outb.pars()->videoWidth = scaledWidth;
	outb.pars()->videoHeight = scaledHeight;
	outb.pars()->v4l2PixelFormat = pixFmt;

	return newOutputBuffer(0, outb);
}
