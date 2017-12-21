#ifndef QTVIDEOOUTPUT_H
#define QTVIDEOOUTPUT_H

#include <lmm/rawbuffer.h>
#include <lmm/baselmmelement.h>

#include <QMutex>
#include <QtWidgets/QWidget>

class QTimer;
class OverlayInfo;

typedef void (*paintHook)(void *sender, void *priv, const RawBuffer &);

class VideoWidget : public QWidget
{
	Q_OBJECT
public:
	VideoWidget(QWidget *parent);

	enum SyncMethod {
		IMMEDIATE,
		TIMER,
		TIMESTAMP,
	};

	enum OverlayType {
		HISTOGRAM,
		TEXT,
		POLYGON,
	};

	void paintBuffer(const RawBuffer &buf);
	void addStaticOverlay(const QString &line, const QString &family = "Courier", int size = 16, int weight = 8, QColor color = Qt::yellow, QRectF pos = QRectF(0.1, 0.1, 1.0, 1.0));
	void setStatusOverlay(const QString &text);
	int addGraphicOverlay(OverlayType type, int x, int y, int w, int h, float opacity);
	int addGraphicOverlay(const QPolygon &polygon, QColor color);
	void * getOverlayObject(int index);
	int setOverlay(int index, const QPolygon &polygon);
	void setPaintHook(paintHook hook, void *priv) { _paintHook = hook; _paintHookPriv = priv; }
	int getDropCount() { return dropCount; }
	int getRenderCount() { return renderCount; }
	int getLastBufferNo() { return lastBufferNo; }
	void setFrameStats(const QString &text, QColor color = Qt::yellow);
	int getBufferCount();
	qint64 getLastBuffetTs() { return lastBufferTs; }
	void setNoVideoImage(const QImage &im);
	void setSynchronizationMethod(SyncMethod m) { sync = m; }
	SyncMethod getSynchronizationMethod() { return sync; }
protected slots:
	void timeout();
protected:
	void paintEvent(QPaintEvent *);
	void keyPressEvent(QKeyEvent *kev);
	qint64 interpolatePts(int ts);
	bool paintOneFrame(QPainter *p);
	bool paintWithTs(QPainter *p);
	void paintBuffer(const RawBuffer &buf, QPainter *p);

	QMutex lock;
	QList<RawBuffer> queue;
	QTimer *playbackTimer;
	SyncMethod sync;
	QList<OverlayInfo *> overlays;
	int statusOverlay;
	int frameStatsOverlay;
	int dropCount;
	int renderCount;
	paintHook _paintHook;
	void *_paintHookPriv;
	qint64 lastBufferTs;
	int lastBufferNo;
	qint64 refWallTime;
	int refTs;
	QImage noVideoImage;
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
