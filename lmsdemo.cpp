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

}

void LmsDemo::exitLater()
{
}

LmsDemo::~LmsDemo()
{
	delete ui;
	delete dec;
}

void LmsDemo::cleanUpAndExit()
{
}

void LmsDemo::on_toolPlay_clicked()
{
	ui->frameBack->setStyleSheet("QFrame#frameBack { background-color: blue;}");
	dec->startDecoding();
}

void LmsDemo::on_toolStop_clicked()
{
	ui->frameBack->setStyleSheet("QFrame#frameBack { background-color: rgb(48, 48, 48);}");
	dec->stopDecoding();
}

void LmsDemo::on_toolPrevPage_clicked()
{
}
