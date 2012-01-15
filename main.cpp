#include <QtGui/QApplication>
#include "lmsdemo.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	LmsDemo w;
	w.show();
	
	return a.exec();
}
