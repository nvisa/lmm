#ifndef QTVIDEOOUTPUT_H
#define QTVIDEOOUTPUT_H

#include <lmm/rawbuffer.h>
#include <lmm/baselmmelement.h>

#include <QMutex>
#include <QtWidgets/QWidget>

class QTimer;
class OverlayInfo;

class VideoWidget : public QWidget
{
	Q_OBJECT
public:
	VideoWidget(QWidget *parent);

	enum SyncMethod {
		IMMEDIATE,
		TIMER
	};

	enum OverlayType {
		HISTOGRAM,
		TEXT,
		STATUS,
	};

	void paintBuffer(const RawBuffer &buf);
	void addStaticOverlay(const QString &line, const QString &family = "Courier", int size = 16, int weight = 8, QColor color = Qt::yellow, QRectF pos = QRectF(0.1, 0.1, 1.0, 1.0));
	void addStatusOverlay(const QString &line, const QString &family = "Courier", int size = 16, int weight = 8, QColor color = Qt::yellow, QRectF pos = QRectF(0.3, 0.3, 1.0, 1.0));
	void setStatusOverlay(const QString &text);
	int addGraphicOverlay(OverlayType type, int x, int y, int w, int h, float opacity);
	void * getOverlayObject(int index);
protected slots:
	void timeout();
protected:
	void paintEvent(QPaintEvent *);

	QMutex lock;
	QList<RawBuffer> queue;
	QTimer *playbackTimer;
	SyncMethod sync;
	QList<OverlayInfo *> overlays;
	int statusOverlay;
};

class QtVideoOutput : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit QtVideoOutput(QObject *parent = 0);
	void setParentWindow(QWidget *p);
	QWidget * parentWindow();
	VideoWidget *getWidget();

protected:
	virtual int processBuffer(const RawBuffer &buf);

	VideoWidget *w;
};

#endif // QTVIDEOOUTPUT_H
