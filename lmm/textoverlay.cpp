#include "textoverlay.h"
#include "rawbuffer.h"

#include <emdesk/debug.h>

#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <QPainter>
#include <QImage>
#include <QRect>
#include <QDateTime>
#include <QTime>
#include <QFile>
#include <QFontMetrics>

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

TextOverlay::TextOverlay(QObject *parent) :
	BaseLmmElement(parent)
{
	mmapfd = -1;
	imageBuf = NULL;
	fontSize = 26;
}

int TextOverlay::start()
{
	BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
	imageBuf = Buffer_create(1280 * 240 * 3, BufferGfx_getBufferAttrs(&gfxAttrs));
	if (!imageBuf) {
		mDebug("Unable to allocate memory for text image buffer");
		return -ENOMEM;
	}
	createCharMap();
	mmapfd = open("/dev/dm365mmap", O_RDWR | O_SYNC);
	if (mmapfd == -1)
		return -ENOENT;
	return BaseLmmElement::start();
}

int TextOverlay::stop()
{
	if (mmapfd > 0) {
		close(mmapfd);
		mmapfd = -1;
	}
	if (imageBuf)
		Buffer_delete(imageBuf);
	return BaseLmmElement::stop();
}

int TextOverlay::addBuffer(RawBuffer *buffer)
{
	if (buffer) {
		QTime t; t.start();
		//if (charMap.isEmpty())
			//createCharMap(buffer);
		/*QImage image((uchar *)Buffer_getUserPtr(imageBuf),
					 1280, 120, QImage::Format_RGB888);
		memset(Buffer_getUserPtr(imageBuf), 0, Buffer_getSize(imageBuf));
		QPainter painter(&image);
		painter.setPen(Qt::red);
		QFont f = painter.font();
		f.setPointSize(26);
		painter.setFont(f);
		painter.drawText(QRect(0, 0, image.width(), image.height()),
						 QDateTime::currentDateTime().toString());*/

#if 0
		char *data = (char *)buffer->data() + 1280 * 150;
		for (int j = 0; j < image.height(); j++) {
			int start = j * 1280;
			for (int i = 0; i < image.width(); i++) {
				int val = qRed(image.pixel(i, j));
				if (val)
					data[start + i] = val;
			}
		}
#else
		char *dst = (char *)buffer->data() + 1280 * 150;
		QString text = "testaksldjjasdfhlkjasdfh";
		QByteArray ba = text.toLatin1();
		for (int i = 0; i < ba.size(); i++) {
			const QByteArray map = charMap[(int)ba.at(i) - 32];
			int w = map.size() / fontHeight;
			const char *src = map.constData();
			for (int j = 0; j < fontHeight; j++) {
				memcpy(dst + 1280 * j, src + w * j, w);
			}
			dst += w;
		}

#endif
		qDebug() << "overlay took" << t.elapsed();
	}
	return 0;//BaseLmmElement::addBuffer(buffer);
}

void TextOverlay::createCharMap()
{
	QImage image((uchar *)Buffer_getUserPtr(imageBuf),
				 240, 120, QImage::Format_RGB888);
	memset(Buffer_getUserPtr(imageBuf), 0, Buffer_getSize(imageBuf));
	QPainter painter(&image);
	painter.setPen(Qt::red);
	QFont f = painter.font();
	f.setPointSize(fontSize);
	QFontMetrics fm(f);
	painter.setFont(f);
	fontHeight = fm.height();
	for (int i = 32; i < 127; i++) {
		int w = fm.width(QChar(i));
		QByteArray ba(fontHeight * w, 0);
		image.fill(0);
		painter.drawText(QRect(0, 0, image.width(), image.height()), QString(QChar(i)));
		for (int i = 0; i < w; i++)
			for (int j = 0; j < fontHeight; j++)
				ba[j * w + i] = qRed(image.pixel(i, j));
		charMap << ba;

	}
}

int TextOverlay::dmaCopy(void *src, void *dst, QImage *im)
{
	dm365mmap_params params;
	params.src = (uint)src;
	params.srcmode = 0;
	params.srcbidx = 3;

	params.dst = (uint)dst;
	params.dstmode = 0;
	params.dstbidx = 1;

	params.acnt = 1;
	params.bcnt = im->width() * im->height();
	params.ccnt = 1;
	params.bcntrld = im->width();
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
