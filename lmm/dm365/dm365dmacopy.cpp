#include "dm365dmacopy.h"

#include <lmm/debug.h>
#include <lmm/dmai/dmaibuffer.h>

#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define DM365MMAP_IOCMEMCPY        0x7
#define DM365MMAP_IOCWAIT          0x8
#define DM365MMAP_IOCCLEAR_PENDING 0x9

struct dm365mmap_params {
	unsigned long src;
	unsigned long dst;
	unsigned int  srcmode;
	unsigned int  srcfifowidth;
	int           srcbidx;
	int           srccidx;
	unsigned int  dstmode;
	unsigned int  dstfifowidth;
	int           dstbidx;
	int           dstcidx;
	int           acnt;
	int           bcnt;
	int           ccnt;
	int           bcntrld;
	int           syncmode;
};

DM365DmaCopy::DM365DmaCopy(QObject *parent) :
	BaseLmmElement(parent)
{
	mmapfd = -1;
	bufferCount = 0;
	addNewOutputChannel();
}

int DM365DmaCopy::dmaCopy(void *src, void *dst, int acnt, int bcnt)
{
	if (mmapfd == -1) {
		mmapfd = open("/dev/dm365mmap", O_RDWR | O_SYNC);
		if (mmapfd == -1)
			return -ENOENT;
	}
	dm365mmap_params params;
	params.src = (uint)src;
	params.srcmode = 0;
	params.srcbidx = acnt;

	params.dst = (uint)dst;
	params.dstmode = 0;
	params.dstbidx = acnt;

	params.acnt = acnt;
	params.bcnt = bcnt;
	params.ccnt = 1;
	params.bcntrld = acnt; //not valid for AB synced
	params.syncmode = 1; //AB synced

	int err = 0;
	dmalock.lock();
	if (ioctl(mmapfd, DM365MMAP_IOCMEMCPY, &params) == -1) {
		mDebug("error %d during dma copy", errno);
		err = errno;
	}
	dmalock.unlock();
	mInfo("dma copy succeeded with %d", err);
	return err;
}

void DM365DmaCopy::aboutToDeleteBuffer(const RawBufferParameters *pars)
{
	buflock.lock();
	freeBuffers << DmaiBuffer(pool.first().getMimeType(), (Buffer_Handle)pars->dmaiBuffer, this);
	buflock.unlock();
}

int DM365DmaCopy::processBuffer(const RawBuffer &buf)
{
	if (!bufferCount) {
		newOutputBuffer(0, buf);
		return newOutputBuffer(1, createAndCopy(buf));
	}
	/* take buffers from a local pool */
	if (!pool.size()) {
		BufferGfx_Attrs *attrs = DmaiBuffer::createGraphicAttrs(
					buf.constPars()->videoWidth,
					buf.constPars()->videoHeight,
					buf.constPars()->v4l2PixelFormat);
		for (int i = 0; i < bufferCount; i++) {
			DmaiBuffer dmaibuf("video/x-raw-yuv", buf.size(), attrs, this);
			pool << dmaibuf;
			pool[i].pars()->captureTime = 0;
			pool[i].pars()->fps = 1;
			memset(pool[i].data(), 0, buf.size());
			freeBuffers << DmaiBuffer(dmaibuf.getMimeType(), (Buffer_Handle)dmaibuf.constPars()->dmaiBuffer, this);
		}
		delete attrs;
	}
	buflock.lock();
	if (!freeBuffers.size()) {
		buflock.unlock();
		qDebug() << "no free buffers left";
		newOutputBuffer(0, buf);
		return 0;
	}
	DmaiBuffer dstBuf = freeBuffers.takeFirst();
	buflock.unlock();
	int err = dmaCopy((void *)Buffer_getPhysicalPtr((Buffer_Handle)buf.constPars()->dmaiBuffer)
						 , (void *)Buffer_getPhysicalPtr((Buffer_Handle)dstBuf.constPars()->dmaiBuffer)
						 , buf.constPars()->videoWidth, buf.constPars()->videoHeight * 3 / 2);
	dstBuf.pars()->captureTime = buf.constPars()->captureTime;
	newOutputBuffer(0, buf);
	if (err)
		return err;
	return newOutputBuffer(1, dstBuf);
}

RawBuffer DM365DmaCopy::createAndCopy(const RawBuffer &buf)
{
	BufferGfx_Attrs *attrs = DmaiBuffer::createGraphicAttrs(
				buf.constPars()->videoWidth,
				buf.constPars()->videoHeight,
				buf.constPars()->v4l2PixelFormat);
	DmaiBuffer dstBuf("video/x-raw-yuv", buf.size(), attrs);
	delete attrs;
	dmaCopy((void *)Buffer_getPhysicalPtr((Buffer_Handle)buf.constPars()->dmaiBuffer)
						 , (void *)Buffer_getPhysicalPtr((Buffer_Handle)dstBuf.constPars()->dmaiBuffer)
						 , buf.constPars()->videoWidth, buf.constPars()->videoHeight * 3 / 2);
	return dstBuf;
}
