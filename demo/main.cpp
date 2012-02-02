#include <QtGui/QApplication>

#include "lmsdemo.h"
#include <lmm/lmmcommon.h>

static LmsDemo *w;

int main(int argc, char *argv[])
{
	LmmCommon::init();
	LmmCommon::installSignalHandlers();
	QApplication a(argc, argv);
	w = new LmsDemo;
	w->show();
	
	return a.exec();
}
