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

DM365DmaCopy::DM365DmaCopy(QObject *parent, int outCnt) :
	BaseLmmElement(parent)
{
	mmapfd = -1;
	bufferCount = 0;
	allocSize = 0;
	outputCount = outCnt;
	mode = MODE_OVERLAY;
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
	/* set used size to 0 so that newly created will buffer use all available data */
	Buffer_setNumBytesUsed((Buffer_Handle)pars->dmaiBuffer, 0);
	freeBuffers << DmaiBuffer(pool.first().getMimeType(), (Buffer_Handle)pars->dmaiBuffer, this);
	buflock.unlock();
}

int DM365DmaCopy::processBuffer(const RawBuffer &buf)
{
	if (mode == MODE_OVERLAY)
		return processBufferOverlay(buf);

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
			if (!allocSize)
				allocSize = buf.size();
			DmaiBuffer dmaibuf(buf.getMimeType(), allocSize, attrs, this);
			pool << dmaibuf;
			pool[i].pars()->captureTime = 0;
			pool[i].pars()->fps = 1;
			memset(pool[i].data(), 0, allocSize);
			freeBuffers << DmaiBuffer(dmaibuf.getMimeType(), (Buffer_Handle)dmaibuf.constPars()->dmaiBuffer, this);
		}
		delete attrs;
	}
	buflock.lock();
	if (!freeBuffers.size()) {
		buflock.unlock();
		mDebug("no free buffers left");
		newOutputBuffer(0, buf);
		return 0;
	}
	DmaiBuffer dstBuf = freeBuffers.takeFirst();
	dstBuf.pars()->streamBufferNo = buf.constPars()->streamBufferNo;
	buflock.unlock();
	int err = 0;
	if (allocSize == buf.size()) {
		err = dmaCopy((void *)Buffer_getPhysicalPtr((Buffer_Handle)buf.constPars()->dmaiBuffer)
							 , (void *)Buffer_getPhysicalPtr((Buffer_Handle)dstBuf.constPars()->dmaiBuffer)
							 , buf.constPars()->videoWidth, buf.constPars()->videoHeight * 3 / 2);
	} else {
		err = dmaCopy((void *)Buffer_getPhysicalPtr((Buffer_Handle)buf.constPars()->dmaiBuffer)
							 , (void *)Buffer_getPhysicalPtr((Buffer_Handle)dstBuf.constPars()->dmaiBuffer)
							 , 1024, buf.size() / 1024 + 1);
		dstBuf.setUsedSize(buf.size());
		if (allocSize < buf.size())
			mDebug("memory buffer too short for input buffer: %d < %d", allocSize, buf.size());
	}
	dstBuf.pars()->captureTime = buf.constPars()->captureTime;
	dstBuf.pars()->encodeTime = buf.constPars()->encodeTime;
	dstBuf.pars()->videoWidth = buf.constPars()->videoWidth;
	dstBuf.pars()->videoHeight = buf.constPars()->videoHeight;
	dstBuf.pars()->frameType = buf.constPars()->frameType;
	if (err)
		return err;
	if (outputCount == 1)
		return newOutputBuffer(0, dstBuf);
	newOutputBuffer(0, buf);
	return newOutputBuffer(1, dstBuf);
}
#include <QElapsedTimer>

static void overlay(char *dst, int x, int y, int w, int h)
{
	if (x > 1920 || y > 1080 || x < 0 || y < 0)
		return;
	w = qMin(1920 - x, w);
	h = qMin(1080 - y, h);
	dst = dst + y * 1920 + x;
	for (int i = 0; i < h; i++)
		memset(dst + 1920 * i, 0, w);
}

int DM365DmaCopy::processBufferOverlay(const RawBuffer &buf)
{
	//DmaiBuffer srcBuf = freeBuffers[0];
	QElapsedTimer t; t.start();
	int acnt = 128, bcnt = 128;

	//char *src = (char *)Buffer_getPhysicalPtr((Buffer_Handle)srcBuf.constPars()->dmaiBuffer);
	char *dst = (char *)buf.constData();//(char *)Buffer_getPhysicalPtr((Buffer_Handle)buf.constPars()->dmaiBuffer) + 1920 * 10;

	olock.lock();
	for (int i = 0; i < overlays.size(); i++) {
		const QRect r = overlays[i];
		//overlay(dst, r.x(), r.y(), r.width(), r.height());
	}
	olock.unlock();
	/*overlay(dst, 0, 0, 128, 128);
	overlay(dst, 128, 512, 128, 128);
	overlay(dst, 256, 600, 128, 128);
	overlay(dst, 512, 300, 128, 128);
	overlay(dst, 800, 300, 128, 128);
	overlay(dst, 1000, 300, 128, 128);
	overlay(dst, 1200, 300, 128, 128);
	overlay(dst, 1400, 300, 128, 128);*/

	/*dmaCopy(src, dst, acnt, bcnt);
	dmaCopy(src, dst + 128, acnt, bcnt);
	dmaCopy(src, dst + 128 * 2, acnt, bcnt);
	dmaCopy(src, dst + 128 * 2, acnt, bcnt);
	dmaCopy(src, dst + 128 * 2, acnt, bcnt);*/
	//ffDebug() << t.elapsed();

	return newOutputBuffer(0, buf);
}

RawBuffer DM365DmaCopy::createAndCopy(const RawBuffer &buf)
{
	BufferGfx_Attrs *attrs = DmaiBuffer::createGraphicAttrs(
				buf.constPars()->videoWidth,
				buf.constPars()->videoHeight,
				buf.constPars()->v4l2PixelFormat);
	DmaiBuffer dstBuf("video/x-raw-yuv", buf.size(), attrs);
	dstBuf.setParameters(buf.constPars());
	delete attrs;
	dmaCopy((void *)Buffer_getPhysicalPtr((Buffer_Handle)buf.constPars()->dmaiBuffer)
						 , (void *)Buffer_getPhysicalPtr((Buffer_Handle)dstBuf.constPars()->dmaiBuffer)
						 , buf.constPars()->videoWidth, buf.constPars()->videoHeight * 3 / 2);
	return dstBuf;
}

void DM365DmaCopy::setAllocateSize(int size)
{
	allocSize = size;
	if (allocSize) {
		BufferGfx_Attrs *attrs = DmaiBuffer::createGraphicAttrs(0, 0, 0); //some dummy attributes
		for (int i = 0; i < bufferCount; i++) {
			DmaiBuffer dmaibuf("video/x-raw-yuv", allocSize, attrs, this);
			pool << dmaibuf;
			pool[i].pars()->captureTime = 0;
			pool[i].pars()->fps = 1;
			memset(pool[i].data(), 0, allocSize);
			freeBuffers << DmaiBuffer(dmaibuf.getMimeType(), (Buffer_Handle)dmaibuf.constPars()->dmaiBuffer, this);
		}
		delete attrs;
	}
}

void DM365DmaCopy::addOverlay(int x, int y, int w, int h)
{
	QRect r(x, y, w, h);
	olock.lock();
	overlays << r;
	olock.unlock();
}

void DM365DmaCopy::setOverlay(int index, int x, int y, int w, int h)
{
	olock.lock();
	overlays[index] = QRect(x, y, w, h);
	olock.unlock();
}

void DM365DmaCopy::setOverlay(int index, const QRect &r)
{
	olock.lock();
	overlays[index] = r;
	olock.unlock();
}

QRect DM365DmaCopy::getOverlay(int index)
{
	QMutexLocker l(&olock);
	return overlays[index];
}
