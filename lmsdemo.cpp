#include "lmsdemo.h"
#include "ui_lmsdemo.h"
#include "lmm/avidecoder.h"
#include "lmm/dvbplayer.h"
#include "lmm/baselmmelement.h"
#include "lmm/baselmmplayer.h"
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

	dec = new DvbPlayer;
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
		ui->frameBack->hide();
		hideCounter = -1;
	}
	if (labelHideCounter > 0) {
		labelHideCounter--;
		updateVirtPosition(pos);
	} else {
		ui->labelVirtPos->setVisible(false);
	}
	if (ui->toolMoviePage->isChecked())
		showDecodeInfo();
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

void LmsDemo::addElement(BaseLmmElement *el)
{
	int x, y, spacing = 20, rectW = 150, rectH = 100;
	QString className = el->metaObject()->className();
	if (className == "AviDemux") {
		x = 10;
		y = scene->height() / 2 - rectH / 2;
	} else if (className == "DmaiDecoder") {
		x = 10 + rectW + 10;
		y = 10;
	} else if (className == "FbOutput") {
		x = 10 + 2 * rectW + 20;
		y = 10;
	} else if (className == "Mad") {
		x = 10 + rectW + 10;
		y = scene->height() - rectH - 10;
	} else if (className == "AlsaOutput") {
		x = 10 + 2 * rectW + 20;
		y = scene->height() - rectH - 10;
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

	scene->addItem(rectItem);
	scene->addItem(name);
	scene->addItem(inBufCnt);
	scene->addItem(outBufCnt);
	scene->addItem(inCnt);
	scene->addItem(outCnt);

	visuals[el] << inBufCnt;
	visuals[el] << outBufCnt;
	visuals[el] << inCnt;
	visuals[el] << outCnt;
}

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
		visuals[el][0]->setText(QString("inputBuffers: %1").arg(el->getInputBufferCount()));
		visuals[el][1]->setText(QString("outpuBuffers: %1").arg(el->getOutputBufferCount()));
		visuals[el][2]->setText(QString("receivedBuffers: %1").arg(el->getReceivedBufferCount()));
		visuals[el][3]->setText(QString("sentBuffers: %1").arg(el->getSentBufferCount()));
	}
}

void LmsDemo::on_toolPlay_clicked()
{
	if (!dec->play()) {
		ui->frameBack->setStyleSheet("QFrame#frameBack { background-color: blue;}");
		HardwareOperations::blendOSD(true, 31);
		timer->start(100);
		ui->sliderPosition->setMaximum(dec->getDuration() / 1000000);
		ui->sliderPosition->setValue(0);
		hideCounter = HIDE_COUNT;
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
		/*if (ev->type() == QEvent::MouseButtonPress) {
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
		}*/
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
