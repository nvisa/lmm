#ifndef FBOUTPUT_H
#define FBOUTPUT_H

#include <lmm/baselmmoutput.h>

#include <QList>
#include <QVariant>

class RawBuffer;
class QTime;
class BaseLmmElement;

class FbOutput : public BaseLmmOutput
{
	Q_OBJECT
public:
	explicit FbOutput(QObject *parent = 0);
	virtual int outputBuffer(RawBuffer buf);
	virtual int start();
	virtual int stop();
	virtual int flush();

	void * getFbAddr() { return fbAddr; }
signals:
	
public slots:
protected:
	int fd;
	int fbSize;
	unsigned char *fbAddr;
	int fbLineLen;
	int fbHeight;

	int openFb(QString filename);
};

#endif // FBOUTPUT_H
