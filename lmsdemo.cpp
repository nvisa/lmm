#include "lmsdemo.h"
#include "ui_lmsdemo.h"

#include "lmm/avidecoder.h"
#include "emdesk/emdeskwindowmanager.h"
#define DEBUG
#include "emdesk/debug.h"

LmsDemo::LmsDemo(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::LmsDemo)
{
	ui->setupUi(this);
	emdeskWindowManager::addWindow(this);

	AviDecoder *dec = new AviDecoder;
	dec->startDecoding();
}

LmsDemo::~LmsDemo()
{
	delete ui;
}
