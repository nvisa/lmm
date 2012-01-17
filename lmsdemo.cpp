#include "lmsdemo.h"
#include "ui_lmsdemo.h"
#include "lmm/avidecoder.h"
#include "emdesk/emdeskwindowmanager.h"
#define DEBUG
#include "emdesk/debug.h"

#include <QTimer>

LmsDemo::LmsDemo(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::LmsDemo)
{
	ui->setupUi(this);
	emdeskWindowManager::addWindow(this);

	dec = new AviDecoder;
	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
	enableSliderUpdate = true;
	hideCounter = -1;
	labelHideCounter = -1;
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

void LmsDemo::on_toolPlay_clicked()
{
	ui->frameBack->setStyleSheet("QFrame#frameBack { background-color: blue;}");
	dec->startDecoding();
	timer->start(100);
	ui->sliderPosition->setMaximum(dec->getDuration() / 1000000);
	ui->sliderPosition->setValue(0);
}

void LmsDemo::on_toolStop_clicked()
{
	ui->frameBack->setStyleSheet("QFrame#frameBack { background-color: rgb(48, 48, 48);}");
	dec->stopDecoding();
	timer->stop();
}

void LmsDemo::on_toolPrevPage_clicked()
{
}
