#include <QCoreApplication>

#include "lmm/dmaidecoder.h"
#include "emdesk/debug.h"


int main(int argc, char *argv[])
{
	initDebug();
	DmaiDecoder::initCodecEngine();
	QCoreApplication a(argc, argv);
	qDebug() << "starting";
	return a.exec();
}
