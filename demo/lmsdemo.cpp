#include "lmsdemo.h"
#include "ui_lmsdemo.h"
#include "../lmm/baselmmdemux.h"
#include "../lmm/dvbplayer.h"
#include "../lmm/avidecoder.h"
#include "../lmm/mp3player.h"
#include "../lmm/baselmmelement.h"
#include "../lmm/baselmmplayer.h"
#include "../lmm/circularbuffer.h"
#include "emdesk/emdeskwindowmanager.h"
#include "emdesk/hardwareoperations.h"
#include "emdesk/debug.h"

#include <QTimer>
#include <QMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QBrush>
#include <QMetaObject>

#define HIDE_COUNT 40

LmsDemo::LmsDemo(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::LmsDemo)
{
	ui->setupUi(this);
	emdeskWindowManager::addWindow(this);

	dec = new AviPlayer;
	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
	enableSliderUpdate = true;
	hideCounter = -1;
	labelHideCounter = -1;
	ui->sliderPosition->installEventFilter(this);
	ui->sliderVolume->installEventFilter(this);
	ui->frameMask->installEventFilter(this);
	scene = NULL;
	ui->sliderVolume->setValue(dec->getVolumeLevel());
}

void LmsDemo::exitLater()
{
}

LmsDemo::~LmsDemo()
{
	delete ui;
	delete dec;
}

void LmsDemo::timeout()
{
	if (ui->sliderPosition->maximum() == 0)
		ui->sliderPosition->setMaximum(dec->getDuration() / 1000000);
	int duration = ui->sliderPosition->maximum();
	int pos = dec->getPosition() / 1000000;
	ui->labelDuration->setText(QString("%1:%2:%3 / %4:%5:%6")
							   .arg(QString::number(pos / 3600).rightJustified(2,'0'))
							   .arg(QString::number((pos % 3600) / 60).rightJustified(2,'0'))
							   .arg(QString::number(pos % 60).rightJustified(2,'0'))
							   .arg(QString::number(duration / 3600).rightJustified(2,'0'))
							   .arg(QString::number((duration % 3600) / 60).rightJustified(2,'0'))
							   .arg(QString::number(duration % 60).rightJustified(2,'0')));
	if (enableSliderUpdate)
		ui->sliderPosition->setValue(pos);
	if (hideCounter > 0) {
		hideCounter--;
	} else if (hideCounter == 0) {
		if (ui->stackedWidget->currentIndex() != 1)
			ui->frameBack->hide();
		hideCounter = -1;
	}
	if (labelHideCounter > 0) {
		labelHideCounter--;
		updateVirtPosition(pos);
	} else {
		ui->labelVirtPos->setVisible(false);
	}
	if (ui->toolMoviePage->isChecked()) {
		/*
		 * Refreshing information at every 100 msec is fancy
		 * but it causes too much overhead, so we refresh it
		 * at every 1 sec
		 */
		static int count = 10;
		if (count-- == 0) {
			showDecodeInfo();
			count = 10;
		}
	}
}

void LmsDemo::cleanUpAndExit()
{
}

void LmsDemo::updateVirtPosition(int val)
{
	int h = val / 3600;
	int m = (val - h * 3600) / 60;
	int s = val - h * 3600 - m * 60;
	if (!h) {
		ui->labelVirtPos->setText(QString("%1:%2")
								  .arg(QString::number(m).rightJustified(2, '0'))
								  .arg(QString::number(s).rightJustified(2, '0')));
	} else {
		ui->labelVirtPos->setText(QString("%1:%2:%3")
								  .arg(QString::number(h).rightJustified(2, '0'))
								  .arg(QString::number(m).rightJustified(2, '0'))
								  .arg(QString::number(s).rightJustified(2, '0')));
	}
}

#define DEMUX_X (10 + 10 + rectW)
void LmsDemo::addElement(BaseLmmElement *el)
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

/*
 * Note that this function has an overhead of ~10 msec
 */
void LmsDemo::showDecodeInfo()
{
	const QList<BaseLmmElement *> elements = dec->getElements();
	if (!scene) {
		/* init */
		scene = new QGraphicsScene(ui->graphicsView);
		scene->setSceneRect(0, 0, ui->graphicsView->width() - 10, ui->graphicsView->height() - 10);

		//int offX = 10, offY = 10;
		foreach (BaseLmmElement *el, elements) {
			visuals.insert(el, QList<QGraphicsSimpleTextItem*>());
			addElement(el);
		}

		ui->graphicsView->setScene(scene);
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
}

void LmsDemo::on_toolPlay_clicked()
{
	if (!dec->play("/media/card/KamiL Koç ÇeviriLmiş FiLmLer/4.Dönem Lisanslı Filmler/Yabancı Filmler/Macera/Kanunsuzlar.avi")) {
	//if (!dec->play("/media/net/Fringe.S04E06.HDTV.XviD-LOL.[VTV].avi")) {
		ui->frameBack->setStyleSheet("QFrame#frameBack { background-color: blue;}");
		HardwareOperations::blendOSD(true, 31);
		timer->start(100);
		ui->sliderPosition->setMaximum(0);
		ui->sliderPosition->setValue(0);
		hideCounter = HIDE_COUNT;

		/* sound bar */
		ui->sliderVolume->setMaximum(100);
		ui->sliderVolume->setValue(dec->getVolumeLevel());
	}
}

void LmsDemo::on_toolStop_clicked()
{
	if (!dec->stop()) {
		ui->frameBack->setStyleSheet("QFrame#frameBack { background-color: rgb(48, 48, 48);}");
		HardwareOperations::blendOSD(false);
		timer->stop();
	}
}

void LmsDemo::on_toolPrevPage_clicked()
{
}

bool LmsDemo::eventFilter(QObject *obj, QEvent *ev)
{
	if (obj == ui->frameMask && ev->type() == QEvent::MouseButtonPress) {
		ui->frameBack->show();
		hideCounter = HIDE_COUNT;
		return true;
	} else if (obj == ui->sliderPosition) {
		if (ev->type() == QEvent::MouseButtonPress) {
			QMouseEvent *mev = (QMouseEvent *)ev;
			ui->labelVirtPos->setVisible(true);
			int val = ui->sliderPosition->maximum() * mev->x() / ui->sliderPosition->width();
			updateVirtPosition(val);
		} else if (ev->type() == QEvent::MouseButtonRelease) {
			QMouseEvent *mev = (QMouseEvent *)ev;
			labelHideCounter = HIDE_COUNT;
			int val = ui->sliderPosition->maximum() * mev->x() / ui->sliderPosition->width();
			dec->seekTo(val * 1000000ll);
			updateVirtPosition(val);
		}
	} else if (obj == ui->sliderVolume) {
		if (ev->type() == QEvent::MouseButtonPress) {
		} else if (ev->type() == QEvent::MouseButtonRelease) {
			QMouseEvent *mev = (QMouseEvent *)ev;
			labelHideCounter = HIDE_COUNT;
			int val = 100 - ui->sliderVolume->maximum() * mev->y() / ui->sliderVolume->height();
			dec->setVolumeLevel(val);
		}
	}

	return false;
}

void LmsDemo::on_toolForward_clicked()
{
	dec->seek(60000000);
}

void LmsDemo::on_toolBackward_clicked()
{
	dec->seek(-60000000);
}

void LmsDemo::on_toolMoviePage_clicked()
{
	if (ui->toolMoviePage->isChecked())
		ui->stackedWidget->setCurrentIndex(1);
	else
		ui->stackedWidget->setCurrentIndex(0);
}

void LmsDemo::on_toolMute_clicked()
{
	dec->setMute(!ui->toolMute->isChecked());
}
