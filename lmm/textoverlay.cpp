#include "textoverlay.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "dmai/cpuload.h"

#include "debug.h"

#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include <QPainter>
#include <QImage>
#include <QRect>
#include <QDateTime>
#include <QTime>
#include <QFile>
#include <QSemaphore>
#include <QDataStream>
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

TextOverlay::TextOverlay(overlayType t, QObject *parent) :
	BaseLmmElement(parent)
{
	type = t;
	mmapfd = -1;
	dmaBuf = NULL;
	fontSize = 28;
	useDma = false;
	imageBuf = NULL;
}

int TextOverlay::setFontSize(int size)
{
	fontSize = size;
	/* refresh maps */
	maplock.lock();
	charMap.clear();
	charPixelMap.clear();
	charFontWidth.clear();
	readMapsFromCache();
	mDebug("new size is %d", size);
	maplock.unlock();
	return 0;
}

int TextOverlay::setOverlayText(QString text)
{
	overlayText = text;
	overlayFields.clear();
	overlayFieldTexts.clear();
	return 0;
}

void TextOverlay::addOverlayField(TextOverlay::overlayTextFields f, QString val)
{
	overlayFields << f;
	overlayFieldTexts << val;
}

int TextOverlay::setOverlayField(int pos, TextOverlay::overlayTextFields f)
{
	if (pos >= overlayFields.size())
		return -ENOENT;
	qDebug("setting field %d type to %d", pos, f);
	overlayFields[pos] = f;
	return 0;
}

int TextOverlay::setOverlayFieldText(int pos, QString text)
{
	if (pos >= overlayFieldTexts.size())
		return -ENOENT;
	overlayFieldTexts[pos] = text;
	return 0;
}

int TextOverlay::start()
{
	if (useDma) {
		BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
		dmaBuf = Buffer_create(1280 * 240 * 3, BufferGfx_getBufferAttrs(&gfxAttrs));
		if (!dmaBuf) {
			mDebug("Unable to allocate memory for text image buffer");
			return -ENOMEM;
		}
		imageBuf = (void *)Buffer_getUserPtr(dmaBuf);
	}
	if (type == CHAR_MAP || type == PIXEL_MAP) {
		if (!readMapsFromCache())
			createYuvMap();
	}
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
	if (dmaBuf)
		Buffer_delete(dmaBuf);
	return BaseLmmElement::stop();
}

int TextOverlay::addBuffer(RawBuffer buffer)
{
	int err = BaseLmmElement::addBuffer(buffer);
	if (err)
		return err;
	/* we modify buffers in-place */
	if (type == CHAR_MAP)
		yuvSwMapOverlay(buffer);
	else if (type == PIXEL_MAP)
		yuvSwPixmapOverlay(buffer);
	inputBuffers.removeFirst();
	outputBuffers << buffer;
	bufsem[0]->release();
	return 0;
}

bool TextOverlay::readMapsFromCache()
{
	QFile f("/etc/lmm/.map.cache");
	if (!f.exists())
		return false;
	if (!f.open(QIODevice::ReadOnly))
		return false;
	QDataStream in(&f);
	qint32 cnt;
	in >> cnt;
	mDebug("%d character maps present", cnt);
	if (fontSize - 8 > cnt)
		fontSize = cnt - 8;
	int index = fontSize - 8;
	for (int i = 0; i < index; i++) {
		in >> charFontWidth;
		in >> charMap;
		in >> charPixelMap;
		charFontWidth.clear();
		charMap.clear();
		charPixelMap.clear();
	}
	in >> charFontWidth;
	in >> charMap;
	in >> charPixelMap;
	f.close();
	if (charFontWidth.size() == 0)
		return false;
	return true;
}

QByteArray TextOverlay::createCharMap(int fontWidth, const QImage &image)
{
	int w = fontWidth;
	QByteArray ba(fontHeight * w, 0);
	for (int i = 0; i < w; i++)
		for (int j = 0; j < fontHeight; j++)
			ba[j * w + i] = qRed(image.pixel(i, j));
	return ba;
}

QList<int> TextOverlay::createPixelMap(int fontWidth, const QImage &image)
{
	QList<int> pixmap;
	int w = fontWidth;
	for (int i = 0; i < w; i++) {
		for (int j = 0; j < fontHeight; j++) {
			int val = qRed(image.pixel(i, j));
			if (val > 0)
				pixmap << (val | (i << 8) | (j << 19));
		}
	}
	return pixmap;
}

void TextOverlay::createYuvMap()
{
	QImage image;
	if (imageBuf)
		image = QImage((uchar *)imageBuf,
				 240, 120, QImage::Format_RGB888);
	else
		image = QImage(240, 240, QImage::Format_RGB888);
	QPainter painter(&image);
	painter.setPen(Qt::red);
	QFont f = painter.font();
	f.setPointSize(fontSize);
	painter.setFont(f);
	QFontMetrics fm = painter.fontMetrics();
	fontHeight = fm.height();
	for (int i = 32; i < 127; i++) {
		QChar ch = QChar(i);
		image.fill(0);
		painter.drawText(QRect(0, 0, image.width(), image.height()),
						 QString(ch));
		int w = fm.width(ch);
		if (type == CHAR_MAP)
			charMap << createCharMap(w, image);
		else if (type == PIXEL_MAP)
			charPixelMap << createPixelMap(w, image);
		charFontWidth << w;
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

void TextOverlay::yuvSwOverlay(RawBuffer buffer)
{
	int pixfmt = buffer.getBufferParameter("v4l2PixelFormat").toInt();
	int width = buffer.getBufferParameter("width").toInt();
	int height = buffer.getBufferParameter("height").toInt();
	int linelen = width;
	if (pixfmt == V4L2_PIX_FMT_UYVY)
		linelen *= 2;
	if (overlayPos.y() > height - fontHeight)
		overlayPos.setY(height - fontHeight);
	QImage image(width, 120, QImage::Format_RGB888);
	QPainter painter(&image);
	painter.setPen(Qt::red);
	QFont f = painter.font();
	f.setPointSize(fontSize);
	painter.setFont(f);
	painter.drawText(QRect(0, 0, image.width(), image.height()),
					 compileOverlayText());
	char *data = (char *)buffer.data() + linelen * height;
	for (int j = 0; j < image.height(); j++) {
		int start = j * linelen + overlayPos.x();
		for (int i = 0; i < image.width(); i++) {
			int val = qRed(image.pixel(i, j));
			if (start + i > linelen)
				break;
			if (val)
				data[start + i] = val;
		}
	}
}

void TextOverlay::yuvSwMapOverlay(RawBuffer buffer)
{
	int pixfmt = buffer.getBufferParameter("v4l2PixelFormat").toInt();
	int width = buffer.getBufferParameter("width").toInt();
	int height = buffer.getBufferParameter("height").toInt();
	int linelen = width;
	if (pixfmt == V4L2_PIX_FMT_UYVY)
		linelen *= 2;
	if (overlayPos.y() > height - fontHeight)
		overlayPos.setY(height - fontHeight);
	char *dst = (char *)buffer.data() + linelen * overlayPos.y();
	QString text = compileOverlayText();
	QByteArray ba = text.toLatin1();
	for (int i = 0; i < ba.size(); i++) {
		const QByteArray map = charMap[(int)ba.at(i) - 32];
		int w = map.size() / fontHeight;
		const char *src = map.constData();
		for (int j = 0; j < fontHeight; j++) {
			if (overlayPos.x() + w <= linelen)
				memcpy(dst + linelen * j + overlayPos.x(), src + w * j, w);
		}
		dst += w;
	}
}

void TextOverlay::yuvSwPixmapOverlay(RawBuffer buffer)
{
	int pixfmt = buffer.getBufferParameter("v4l2PixelFormat").toInt();
	int width = getParameter("videoWidth").toInt();
	int height = getParameter("videoHeight").toInt();
	int linelen = width;
	if (pixfmt == V4L2_PIX_FMT_UYVY)
		linelen *= 2;
	if (overlayPos.y() > height - fontHeight)
		overlayPos.setY(height - fontHeight);
	char *dst = (char *)buffer.data() + linelen * overlayPos.y()
			+ overlayPos.x();
	QString text = compileOverlayText();
	QByteArray ba = text.toLatin1();
	int x, y, val;
	for (int i = 0; i < ba.size(); i++) {
		int ch = (int)ba.at(i);
		maplock.lock();
		const QList<int> map = charPixelMap[ch - 32];
		maplock.unlock();
		int w = charFontWidth[ch - 32];
		/* check for buffer overflows */
		if (overlayPos.x() + w > linelen)
			break;
		foreach (int i, map) {
			val = i & 0xff;
			x = (i >> 8) & 0x7ff;
			y = (i >> 19) & 0x7ff;
			dst[linelen * y + x] = val;
		}
		dst += w;
	}
}

QString TextOverlay::compileOverlayText()
{
	QStringList args;
	for (int i = 0; i < overlayFields.size(); i++) {
		switch (overlayFields[i]) {
		case FIELD_CURRENT_DATE:
			args << QDate::currentDate().toString();
			break;
		case FIELD_CURRENT_TIME:
			args << QTime::currentTime().toString();
			break;
		case FIELD_CURRENT_DATETIME:
			args << QDateTime::currentDateTime().toString();
			break;
		case FIELD_FRAME_NO:
			args << QString::number(receivedBufferCount);
			break;
		case FIELD_STATIC_TEXT:
			args << overlayFieldTexts[i];
			break;
		case FIELD_STREAM_TIME:
			if (streamTime)
				args << QString::number(streamTime->getCurrentTimeMili());
			else
				args << "-1";
			break;
		case FIELD_STREAM_FPS:
			if (streamTime)
				args << QString::number(getFps());
			else
				args << "-1";
			break;
		case FIELD_AVG_CPU_LOAD:
			args << QString::number(CpuLoad::getAverageCpuLoad());
			break;
		default:
			break;
		}
	}
	QString text = overlayText;
	foreach (const QString &arg, args)
		text = text.arg(arg);
	mInfo("compiling text: %s -> %s", qPrintable(overlayText), qPrintable(text));
	return text;
}
