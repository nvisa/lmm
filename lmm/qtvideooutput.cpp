#include "qtvideooutput.h"
#include "debug.h"

#include <QTimer>
#include <QPainter>
#include <QtWidgets/QVBoxLayout>

class OverlayInfo
{
public:
	OverlayInfo()
	{
		color = Qt::yellow;
		fontFamily = "Courier";
		fontSize = 16;
		fontWeight = 8;
		opacity = 1.0;
		r = QRectF(0.1, 0.1, 1.0, 1.0);
		obj = NULL;
	}

	void *obj;
	float opacity;
	QString text;
	QRectF r;
	VideoWidget::OverlayType type;
	QColor color;
	QString fontFamily;
	int fontSize;
	int fontWeight;
};

QtVideoOutput::QtVideoOutput(QObject *parent)
	: BaseLmmElement(parent)
{
	QWidget *pw = qobject_cast<QWidget *>(parent);
	w = new VideoWidget(pw);
	if (!pw) {
		w->setGeometry(0, 0, 1280, 720);
		w->show();
	}
}

void QtVideoOutput::setParentWindow(QWidget *p)
{
	w->setParent(p);
	if (p) {
		if (p->layout())
			p->layout()->addWidget(w);
	} else
		w->setGeometry(0, 0, 1280, 720);
	w->show();
}

QWidget *QtVideoOutput::parentWindow()
{
	return w->parentWidget();
}

VideoWidget *QtVideoOutput::getWidget()
{
	return w;
}

int QtVideoOutput::processBuffer(const RawBuffer &buf)
{
	w->paintBuffer(buf);
	return 0;
}

VideoWidget::VideoWidget(QWidget *parent)
	: QWidget(parent)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	playbackTimer = new QTimer(this);
	connect(playbackTimer, SIGNAL(timeout()), SLOT(timeout()));
	sync = TIMER;
	if (sync == TIMER)
		playbackTimer->start(80);
	statusOverlay = -1;
}

void VideoWidget::paintBuffer(const RawBuffer &buf)
{
	lock.lock();
	queue << buf;
	if (queue.size() > 5) {
		mInfo("dropping frames");
		queue.takeFirst();
	}
	lock.unlock();
	if (sync == IMMEDIATE)
		update();
}

void VideoWidget::addStaticOverlay(const QString &line, const QString &family, int size, int weight, QColor color, QRectF pos)
{
	OverlayInfo *info = new OverlayInfo;
	if (!family.isEmpty())
		info->fontFamily = family;
	if (size)
		info->fontSize = size;
	if (weight)
		info->fontWeight = weight;
	info->color = color;
	info->text = line;
	info->type = VideoWidget::TEXT;
	if (pos.width() && pos.height())
		info->r = pos;
	overlays << info;
}

void VideoWidget::addStatusOverlay(const QString &line, const QString &family, int size, int weight, QColor color, QRectF pos)
{
	if (statusOverlay != -1)
		return;
	OverlayInfo *info = new OverlayInfo;
	if (!family.isEmpty())
		info->fontFamily = family;
	if (size)
		info->fontSize = size;
	if (weight)
		info->fontWeight = weight;
	info->color = color;
	info->text = line;
	info->type = VideoWidget::STATUS;
	if (pos.width() && pos.height())
		info->r = pos;
	statusOverlay = overlays.size();
	overlays << info;
}

void VideoWidget::setStatusOverlay(const QString &text)
{
	if (statusOverlay == -1)
		addStatusOverlay(text);
	overlays[statusOverlay]->text = text;
}

int VideoWidget::addGraphicOverlay(OverlayType type, int x, int y, int w, int h, float opacity)
{
	OverlayInfo *info = new OverlayInfo;
	if (type == HISTOGRAM) {

	}

	info->opacity = opacity;
	info->r = QRect(x, y, w, h);
	info->type = type;
	overlays << info;

	return overlays.size() - 1;
}

void *VideoWidget::getOverlayObject(int index)
{
	if (index >= overlays.size())
		return NULL;
	return overlays[index]->obj;
}

void VideoWidget::timeout()
{
	repaint();
}

void VideoWidget::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	lock.lock();
	if (queue.size()) {
		/* video part */
		const RawBuffer &buf = queue.takeFirst();
		lock.unlock();
		QImage im;
		if (buf.size() == buf.constPars()->videoWidth * buf.constPars()->videoHeight * 3)
			im = QImage((const uchar *)buf.constData(), buf.constPars()->videoWidth, buf.constPars()->videoHeight,
				  QImage::Format_RGB888);
		else
			im = QImage((const uchar *)buf.constData(), buf.constPars()->videoWidth, buf.constPars()->videoHeight,
						QImage::Format_RGB32);
		p.drawImage(rect(), im);
	} else
		lock.unlock();

	/* draw overlay */
	for (int i = 0; i < overlays.size(); i++) {
		OverlayInfo *info = overlays[i];
		if (info->type == VideoWidget::HISTOGRAM) {
			//HistogramPlot *plot = (HistogramPlot *)info->obj;
			//plot->replot();

			//p.setOpacity(info->opacity);
			//p.drawPixmap(info->r.x(), info->r.y(), plot->toPixmap(info->r.width(), info->r.height()));
		} else if (info->type == VideoWidget::TEXT || info->type == VideoWidget::STATUS) {
			if (info->text.isEmpty())
				continue;
			p.setFont(QFont(info->fontFamily, info->fontSize, info->fontWeight));
			p.setPen(info->color);
			QRect r = rect();
			QPoint topLeft(info->r.x() * r.width(), info->r.y() * r.height());
			QPoint bottomRight(r.width() * info->r.width(), r.height() * info->r.height());
			r = QRect(topLeft, bottomRight);
			p.drawText(r, Qt::AlignLeft | Qt::AlignTop, info->text);
		}
	}
}
