#include "qtvideooutput.h"
#include "debug.h"

#include <QTimer>
#include <QPainter>
#include <QKeyEvent>
#include <QtWidgets/QVBoxLayout>

#include <errno.h>

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
	QPolygon polygon;
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
	if (sync == TIMER || sync == TIMESTAMP)
		playbackTimer->start(80);
	statusOverlay = -1;
	frameStatsOverlay = -1;
	dropCount = renderCount = 0;
	_paintHook = NULL;
	refWallTime = 0;
}

void VideoWidget::paintBuffer(const RawBuffer &buf)
{
	lock.lock();
	queue << buf;
	if (queue.size() > 5) {
		mInfo("dropping frames");
		queue.takeFirst();
		dropCount++;
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

void VideoWidget::setStatusOverlay(const QString &text)
{
	if (statusOverlay == -1) {
		addStaticOverlay(text, "Courier", 16, 8, Qt::yellow, QRectF(0.3, 0.3, 1.0, 1.0));
		statusOverlay = overlays.size() - 1;
	}
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

int VideoWidget::addGraphicOverlay(const QPolygon &polygon, QColor color)
{
	OverlayInfo *info = new OverlayInfo;
	info->color = color;
	info->type = VideoWidget::POLYGON;
	info->polygon = polygon;
	overlays << info;
	return overlays.size() - 1;
}

void *VideoWidget::getOverlayObject(int index)
{
	if (index >= overlays.size())
		return NULL;
	return overlays[index]->obj;
}

int VideoWidget::setOverlay(int index, const QPolygon &polygon)
{
	if (index >= overlays.size())
		return -ENOENT;
	if (overlays[index]->type != VideoWidget::POLYGON)
		return -EINVAL;
	overlays[index]->polygon = polygon;
	return 0;
}

void VideoWidget::timeout()
{
	repaint();
}

void VideoWidget::setFrameStats(const QString &text, QColor color)
{
	if (frameStatsOverlay < 0) {
		addStaticOverlay("");
		frameStatsOverlay = overlays.size() - 1;
	}
	OverlayInfo *overlay = overlays[frameStatsOverlay];
	overlay->color = color;
	overlay->text = text;
}

int VideoWidget::getBufferCount()
{
	QMutexLocker l(&lock);
	return queue.size();
}

void VideoWidget::setNoVideoImage(const QImage &im)
{
	noVideoImage = im;
}

void VideoWidget::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	bool painted = false;
	if (sync == TIMER)
		painted = paintOneFrame(&p);
	else if (sync == TIMESTAMP)
		painted = paintWithTs(&p);
	if (!painted && !noVideoImage.isNull())
		p.drawImage(0, 0, noVideoImage);

	/* draw overlay */
	for (int i = 0; i < overlays.size(); i++) {
		OverlayInfo *info = overlays[i];
		if (info->type == VideoWidget::HISTOGRAM) {
			//HistogramPlot *plot = (HistogramPlot *)info->obj;
			//plot->replot();

			//p.setOpacity(info->opacity);
			//p.drawPixmap(info->r.x(), info->r.y(), plot->toPixmap(info->r.width(), info->r.height()));
		} else if (info->type == VideoWidget::TEXT) {
			if (info->text.isEmpty())
				continue;
			p.setFont(QFont(info->fontFamily, info->fontSize, info->fontWeight));
			p.setPen(info->color);
			QRect r = rect();
			QPoint topLeft(info->r.x() * r.width(), info->r.y() * r.height());
			QPoint bottomRight(r.width() * info->r.width(), r.height() * info->r.height());
			r = QRect(topLeft, bottomRight);
			p.drawText(r, Qt::AlignLeft | Qt::AlignTop, info->text);
		} else if (info->type == VideoWidget::POLYGON) {
			p.setPen(info->color);
			p.setBrush(QBrush(info->color));
			if (info->polygon.count() == 2) {
				QColor cinv = QColor(info->color.blue(), info->color.green(), info->color.red(), info->color.alpha());
				p.setPen(cinv);
				p.setBrush(QBrush(cinv));
				QRect r;
				r.setTopLeft(info->polygon[0]);
				r.setBottomRight(info->polygon[1]);
				p.drawRect(r);
			} else
				p.drawPolygon(info->polygon);
		}
	}
}

void VideoWidget::keyPressEvent(QKeyEvent *kev)
{
	if (kev->key() == Qt::Key_P && kev->modifiers() & Qt::ControlModifier) {
		QImage im(geometry().width(), geometry().height(), QImage::Format_ARGB32);
		render(&im);
		im.save(QString("snapshot%1.jpg").arg(lastBufferNo));
	}
}

qint64 VideoWidget::interpolatePts(int ts)
{
	if (!refWallTime) {
		refWallTime = QDateTime::currentMSecsSinceEpoch();
		refTs = ts;
	}

	return (ts - refTs) / 90 + refWallTime;
}

bool VideoWidget::paintOneFrame(QPainter *p)
{
	lock.lock();
	if (queue.size()) {
		/* video part */
		const RawBuffer &buf = queue.takeFirst();
		lock.unlock();
		paintBuffer(buf, p);
		return true;
	} else
		lock.unlock();
	return false;
}

bool VideoWidget::paintWithTs(QPainter *p)
{
	lock.lock();
	while (queue.size()) {
		const RawBuffer &buf = queue.takeFirst();

		qint64 now = QDateTime::currentMSecsSinceEpoch();
		qint64 pts = interpolatePts(buf.constPars()->pts);
		int diff = qAbs(now - pts);
		if (diff > 1000) {
			ffDebug() << "frame too late" << diff;
			dropCount++;
			continue;
		}

		/* we paint one frame and quit */
		lock.unlock();
		paintBuffer(buf, p);
		return true;
	}
	lock.unlock();
	return false;
}

void VideoWidget::paintBuffer(const RawBuffer &buf, QPainter *p)
{
	QImage im;
	if (buf.size() == buf.constPars()->videoWidth * buf.constPars()->videoHeight * 3)
		im = QImage((const uchar *)buf.constData(), buf.constPars()->videoWidth, buf.constPars()->videoHeight,
			  QImage::Format_RGB888);
	else
		im = QImage((const uchar *)buf.constData(), buf.constPars()->videoWidth, buf.constPars()->videoHeight,
					QImage::Format_RGB32);
	p->drawImage(rect(), im);
	lastBufferTs = buf.constPars()->captureTime;
	lastBufferNo = buf.constPars()->streamBufferNo;
	renderCount++;

	if (_paintHook)
		(*_paintHook)(this, _paintHookPriv, buf);
}
