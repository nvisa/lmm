#ifndef FBOUTPUT_H
#define FBOUTPUT_H

#include "baselmmoutput.h"
#include <QList>

class RawBuffer;
class QTime;
class BaseLmmElement;

class FbOutput : public BaseLmmOutput
{
	Q_OBJECT
public:
	explicit FbOutput(QObject *parent = 0);
	int output();
	int start();
	int stop();
signals:
	
public slots:
private:
	int fd;
	int fbSize;
	unsigned char *fbAddr;
	int fbLineLen;
	int fbHeight;

	int openFb(QString filename);
};

#endif // FBOUTPUT_H
