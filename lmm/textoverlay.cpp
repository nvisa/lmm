#include "textoverlay.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "tools/cpuload.h"
#include "debug.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include <QPainter>
#include <QImage>
#include <QRect>
#include <QDateTime>
#include <QFile>
#include <QDataStream>
#include <QFontMetrics>
#include <QElapsedTimer>

TextOverlay::TextOverlay(overlayType t, QObject *parent) :
	BaseLmmElement(parent)
{
	clearBackground = false;
	tzone = 0;
	type = t;
	mmapfd = -1;
	fontSize = 28;
	fontHeight = 0;
	setEnabled(true);
	dateStringFormat = "dd/MM/yyyy hh:mm:ss";
}

int TextOverlay::setFontSize(int size)
{
	clearBackground = false;
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

int TextOverlay::setOverlayPosition(QPoint topLeft)
{
	 overlayPos = topLeft;
	 return 0;
}

int TextOverlay::setOverlayText(QString text)
{
	overlayText = text;
	return 0;
}

void TextOverlay::clearFields()
{
	overlayFields.clear();
	overlayFieldTexts.clear();
}

void TextOverlay::addOverlayField(TextOverlay::overlayTextFields f, QString val)
{
	overlayFields << f;
	overlayFieldTexts << val;
}

QStringList TextOverlay::getFontSizes()
{
	QStringList list;
	for (int i = 0; i < mapCount; i++)
		list << QString::number(i + 8);
	return list;
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
	QMutexLocker l(&fieldLock);
	if (pos >= overlayFieldTexts.size())
		return -ENOENT;
	overlayFieldTexts[pos] = text;
	return 0;
}

int TextOverlay::start()
{
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
	return BaseLmmElement::stop();
}

int TextOverlay::processBuffer(const RawBuffer &buffer)
{
	/* we modify buffers in-place */
	if (isEnabled()) {
		if (type == CHAR_MAP)
			yuvSwMapOverlay(buffer);
		else if (type == PIXEL_MAP)
			yuvSwPixmapOverlay(buffer);
	}
	newOutputBuffer(0, buffer);
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
	mapCount = cnt;
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
	mDebug("creating YUV font map");
	QImage image(240, 240, QImage::Format_RGB888);
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

int TextOverlay::overlayInPlace(const RawBuffer &buffer)
{
	/* we modify buffers in-place */
	if (type == CHAR_MAP)
		yuvSwMapOverlay(buffer);
	else if (type == PIXEL_MAP)
		yuvSwPixmapOverlay(buffer);
	return 0;
}

void TextOverlay::setOverlayTimeZone(int tz)
{
	tzone = tz * 3600;
}

void TextOverlay::setOverlayDateFormat(const QString &format)
{
	dateStringFormat = format;
}

void TextOverlay::yuvSwOverlay(RawBuffer buffer)
{
	int pixfmt = buffer.pars()->v4l2PixelFormat;
	int width = buffer.pars()->videoWidth;
	int height = buffer.pars()->videoHeight;
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
					 compileOverlayText(buffer));
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
	int pixfmt = buffer.pars()->v4l2PixelFormat;
	int width = buffer.pars()->videoWidth;
	int height = buffer.pars()->videoHeight;
	int linelen = width;
	if (pixfmt == V4L2_PIX_FMT_UYVY)
		linelen *= 2;
	if (overlayPos.y() > height - fontHeight)
		overlayPos.setY(height - fontHeight);
	char *dst = (char *)buffer.data() + linelen * overlayPos.y();
	QString text = compileOverlayText(buffer);
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
	int pixfmt = buffer.pars()->v4l2PixelFormat;
	int width = buffer.pars()->videoWidth;
	int height = buffer.pars()->videoHeight;
	int linelen = width;
	if (pixfmt == V4L2_PIX_FMT_UYVY)
		linelen *= 2;
	if (overlayPos.y() > height - fontHeight)
		overlayPos.setY(height - fontHeight);
	char *dst = (char *)buffer.data() + linelen * overlayPos.y()
			+ overlayPos.x();
	QString text = compileOverlayText(buffer);
	QByteArray ba = text.toLatin1();
	int x, y, val;

	if (clearBackground) {
		int hmax = 0, wmax = 0;
		for (int i = 0; i < ba.size(); i++) {
			int ch = (int)ba.at(i);
			if (ch < 32 || ch - 32 >= charPixelMap.size())
				ch = 32;
			const QList<int> map = charPixelMap[ch - 32];
			int w = charFontWidth[ch - 32];
			foreach (int j, map) {
				y = (j >> 19) & 0x7ff;
				x = (j >> 8) & 0x7ff;
				if (y > hmax)
					hmax = y;
			}
			wmax += w;
		}
		wmax += 5 * 2; //margin
		int lmargin = qMin(overlayPos.x(), 5);
		for (int i = 0; i < hmax + 5; i++)
			memset(dst + linelen * i - lmargin, 0, wmax);
	}

	for (int i = 0; i < ba.size(); i++) {
		int ch = (int)ba.at(i);
		if (ch < 32 || ch - 32 >= charPixelMap.size())
			ch = 32;
		maplock.lock();
		const QList<int> map = charPixelMap[ch - 32];
		maplock.unlock();
		int w = charFontWidth[ch - 32];
		/* check for buffer overflows */
		if (overlayPos.x() + w > linelen)
			break;
		foreach (int j, map) {
			val = j & 0xff;
			x = (j >> 8) & 0x7ff;
			y = (j >> 19) & 0x7ff;
			dst[linelen * y + x] = val;
		}
		dst += w;
	}
}

QString TextOverlay::compileOverlayText(const RawBuffer &buf)
{
	QStringList args;
	/* TODO: make timezone parametric */
	QMutexLocker l(&fieldLock);
	for (int i = 0; i < overlayFields.size(); i++) {
		switch (overlayFields[i]) {
		case FIELD_CURRENT_DATE:
			args << QDate::currentDate().toString(dateStringFormat);
			break;
		case FIELD_CURRENT_TIME:
			args << QTime::currentTime().addSecs(tzone).toString(dateStringFormat);
			break;
		case FIELD_CURRENT_DATETIME:
			args << QDateTime::currentDateTime().addSecs(tzone).toString(dateStringFormat);
			break;
		case FIELD_FRAME_NO:
			args << QString::number(getInputQueue(0)->getReceivedCount());
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
				args << QString::number(getOutputQueue(0)->getFps());
			else
				args << "-1";
			break;
		case FIELD_AVG_CPU_LOAD:
			args << QString::number(CpuLoad::getAverageCpuLoad());
			break;
		case FIELD_META:
			args << QString::fromUtf8(buf.constPars()->metaData);
			break;
		case FIELD_FRAME_TIME: {
			qint64 epoch = buf.constPars()->captureTime;
			int secs = epoch / 1000000;
			qint64 usecs = epoch - secs * (qint64)1000000;
			args << QString("%1.%2")
					.arg(QDateTime::fromTime_t(secs).toString("dd-MM-yy_hh:mm:ss"))
					.arg(QString::number(usecs / 1000).rightJustified(3, '0'));
		} default:
			break;
		}
	}
	QString text = overlayText;
	foreach (const QString &arg, args)
		text = text.arg(arg);
	mInfo("compiling text: %s -> %s", qPrintable(overlayText), qPrintable(text));
	return text;
}
