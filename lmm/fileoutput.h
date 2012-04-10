#ifndef FILEOUTPUT_H
#define FILEOUTPUT_H

#include "baselmmoutput.h"

#include <QFuture>
#include <QFutureWatcher>

class QFile;

class FileOutput : public BaseLmmOutput
{
	Q_OBJECT
public:
	explicit FileOutput(QObject *parent = 0);
	int start();
	int stop();
	void setFileName(QString name);
	virtual void signalReceived(int);
signals:
	
public slots:
private:
	QFile *file;
	QString fileName;
	bool isPipe;
	QFutureWatcher<int> *watcher;
	bool pipeClosed;

	int writeBuffer(RawBuffer buf);
	int fifoOutput();
	int outputFunc();
};

#endif // FILEOUTPUT_H
