#include "textoverlay.h"
#include "rawbuffer.h"
#include "streamtime.h"
#include "tools/cpuload.h"
#include "debug.h"
#include "tools/basesettinghandler.h"

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
	type = t;
	mmapfd = -1;
	fontSize = 28;
	setEnabled(true);

	BaseSettingHandler::addHandler("text_overlay", this);
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
	if (type == CHAR_MAP)
		yuvSwMapOverlay(buffer);
	else if (type == PIXEL_MAP)
		yuvSwPixmapOverlay(buffer);
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

int TextOverlay::setSetting(const QString &setting, const QVariant &value)
{
	TextOverlay *overlay = this;
	mInfo("%s: %s", qPrintable(setting), qPrintable(value.toString()));
	if (equals("enabled")) {
		overlay->setEnabled(value.toInt());
	} else if (equals("position")) {
		QStringList l = value.toString().split("x");
		return overlay->setOverlayPosition(QPoint(l.first().toInt(), l.last().toInt()));
	} else if (equals("font_size")) {
		return overlay->setFontSize(value.toInt());
	} else if (equals("font_size_index")) {
		const QStringList list = overlay->getFontSizes();
		if (value.toInt() < list.size())
			overlay->setFontSize(list[value.toInt()].toInt());
	} else if (equals("overlay_text")) {
		return overlay->setOverlayText(value.toString());
	} else if (equals("fields.count")) {
		overlay->clearFields();
		for (int i = 0; i < value.toInt(); i++)
			overlay->addOverlayField(TextOverlay::FIELD_STATIC_TEXT);
	} else if (starts("fields.type")) {
		int f = QString(setting).remove("fields.type").toInt();
		return overlay->setOverlayField(f, (TextOverlay::overlayTextFields)value.toInt());
	} else if (starts("fields.text")) {
		int f = QString(setting).remove("fields.text").toInt();
		return overlay->setOverlayFieldText(f, value.toString());
	} else if (starts("fields.list")) {
		return -EPERM;
	} else
		return -ENOENT;
	return 0;
}

QVariant TextOverlay::getSetting(const QString &setting)
{
	TextOverlay *overlay = this;
	if (equals("enabled"))
		return overlay->isEnabled() ? "1" : "0";
	if (equals("position"))
		return QString("%1x%2").arg(overlay->getOverlayPosition().x())
				.arg(overlay->getOverlayPosition().y());
	if (equals("font_size_list")) {
		return overlay->getFontSizes();
	}
	if (equals("font_size"))
		return overlay->getFontSize();
	if (equals("font_size_index"))
		return overlay->getFontSizes().indexOf(QString::number(overlay->getFontSize()));
	if (equals("overlay_text"))
		return overlay->getOverlayText();
	if (equals("fields.count"))
		return overlay->getFieldCount();
	if (starts("fields.type")) {
		int f = QString(setting).remove("fields.type").toInt();
		if (f < overlay->getFieldCount())
			return overlay->getOverlayField(f);
	}
	if (starts("fields.text")) {
		int f = QString(setting).remove("fields.text").toInt();
		if (f < overlay->getFieldCount())
			return overlay->getOverlayFieldText(f);
	}
	if (equals("fields.list")) {
		QStringList list;
		list << "Current Date";
		list << "Current Time";
		list << "Current Date + Time";
		list << "Frame No";
		list << "Static Text";
		list << "Stream Time";
		list << "Stream FPS";
		list << "CPU Load";
		list << "Capture Time";
		list << "System setting";
		return list;
	}
	return QVariant();
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

QString TextOverlay::compileOverlayText(const RawBuffer &buf)
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
		case FIELD_SETTING:
			args << BaseSettingHandler::getSetting("video_encoding.0.profile").toString();
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
