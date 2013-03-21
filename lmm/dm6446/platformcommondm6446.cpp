#include "platformcommondm6446.h"
#include "dmai/dmaidecoder.h"
#include "hardwareoperations.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QBrush>
#include <QMetaObject>
#include <QCoreApplication>
#include <QMap>

PlatformCommonDM6446::PlatformCommonDM6446()
{
}

void PlatformCommonDM6446::platformInit()
{
	HardwareOperations::writeRegister(0x1c7260c, 0x3004);
	DmaiDecoder::initCodecEngine();
}

void PlatformCommonDM6446::platformCleanUp()
{
	DmaiDecoder::cleanUpDsp();
}

#if 0
static QGraphicsScene *scene = NULL;
static QMap<BaseLmmElement *, QList<QGraphicsSimpleTextItem *> > visuals;
#define DEMUX_X (10 + 10 + rectW)
static void addElement(BaseLmmElement *el)
{
	int x, y, spacing = 20, rectW = 150, rectH = 120;
	QString className = el->metaObject()->className();
	if (el->inherits("BaseLmmDemux")) {
		x = DEMUX_X;
		y = scene->height() / 2 - rectH / 2;
	} else if (className == "DmaiDecoder") {
		x = DEMUX_X + rectW + 10;
		y = 10;
	} else if (className == "FbOutput") {
		x = DEMUX_X + 2 * rectW + 20;
		y = 10;
	} else if (className == "MadDecoder") {
		x = DEMUX_X + rectW + 10;
		y = scene->height() - rectH - 10;
	} else if (className == "AlsaOutput") {
		x = DEMUX_X + 2 * rectW + 20;
		y = scene->height() - rectH - 10;
	} else if (className == "V4l2Input") {
		x = 10;
		y = scene->height() / 2 - rectH / 2;
	}
	QGraphicsRectItem *rectItem = new QGraphicsRectItem();
	rectItem->setRect(x, y, rectW, rectH);
	rectItem->setBrush(QBrush(Qt::yellow));
	x += 5; /* rect left margin */

	QGraphicsSimpleTextItem *name = new QGraphicsSimpleTextItem();
	name->setBrush(QBrush(Qt::black));
	name->setText(el->metaObject()->className());
	name->setPos(x, y);

	y += spacing;
	QGraphicsSimpleTextItem *inBufCnt = new QGraphicsSimpleTextItem();
	inBufCnt->setBrush(QBrush(Qt::black));
	inBufCnt->setPos(x, y);

	y += spacing;
	QGraphicsSimpleTextItem *outBufCnt = new QGraphicsSimpleTextItem();
	outBufCnt->setBrush(QBrush(Qt::black));
	outBufCnt->setPos(x, y);

	y += spacing;
	QGraphicsSimpleTextItem *inCnt = new QGraphicsSimpleTextItem();
	inCnt->setBrush(QBrush(Qt::black));
	inCnt->setPos(x, y);

	y += spacing;
	QGraphicsSimpleTextItem *outCnt = new QGraphicsSimpleTextItem();
	outCnt->setBrush(QBrush(Qt::black));
	outCnt->setPos(x, y);

	y += spacing;
	QGraphicsSimpleTextItem *latency = new QGraphicsSimpleTextItem();
	latency->setBrush(QBrush(Qt::black));
	latency->setPos(x, y);

	scene->addItem(rectItem);
	scene->addItem(name);
	scene->addItem(inBufCnt);
	scene->addItem(outBufCnt);
	scene->addItem(inCnt);
	scene->addItem(outCnt);
	scene->addItem(latency);

	visuals[el] << inBufCnt;
	visuals[el] << outBufCnt;
	visuals[el] << inCnt;
	visuals[el] << outCnt;
	visuals[el] << latency;
}

int PlatformCommonDM6446::showDecodeInfo(QGraphicsView *view, BaseLmmPlayer *dec)
{
	const QList<BaseLmmElement *> elements = dec->getElements();
	if (!scene) {
		/* init */
		scene = new QGraphicsScene(view);
		scene->setSceneRect(0, 0, view->width() - 10, view->height() - 10);

		//int offX = 10, offY = 10;
		foreach (BaseLmmElement *el, elements) {
			visuals.insert(el, QList<QGraphicsSimpleTextItem*>());
			addElement(el);
		}

		view->setScene(scene);
	}
	foreach (BaseLmmElement *el, elements) {
		CircularBuffer *circ = el->getCircularBuffer();
		const QList<QGraphicsSimpleTextItem *> & widgets = visuals[el];
		if (circ) {
			/* circular buffer based element */
			widgets[0]->setText(QString("totalSize: %1 kB").arg(circ->totalSize() >> 10));
			widgets[1]->setText(QString("usedSize: %1 kB").arg(circ->usedSize() >> 10));
			widgets[2]->setText(QString("freeSize: %1 kB").arg(circ->freeSize() >> 10));
		} else {
			BaseLmmDemux *demux = qobject_cast<BaseLmmDemux *>(el);
			if (demux) {
				/* demux elements are special */
				widgets[0]->setText(QString("audioBuffers: %1").arg(demux->audioBufferCount()));
				widgets[1]->setText(QString("videoBuffers: %1").arg(demux->videoBufferCount()));
			} else {
				widgets[0]->setText(QString("inputBuffers: %1").arg(el->getInputBufferCount()));
				widgets[1]->setText(QString("outpuBuffers: %1").arg(el->getOutputBufferCount()));
			}
			widgets[2]->setText(QString("receivedBuffers: %1").arg(el->getReceivedBufferCount()));
			widgets[3]->setText(QString("sentBuffers: %1").arg(el->getSentBufferCount()));
			widgets[4]->setText(QString("latency: %1").arg(el->getLatency()));
		}
	}
	return 0;
}
#endif
