#ifndef FILEOUTPUT_H
#define FILEOUTPUT_H

#include <lmm/baselmmoutput.h>

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
	void setIncremental(bool v) { incremental = v; }
	virtual void signalReceived(int);
signals:
	
public slots:
private:
	QFile *file;
	QString fileName;
	bool isPipe;
	bool pipeClosed;
	bool incremental;

	int writeBuffer(RawBuffer buf);
	int outputBuffer(RawBuffer buf);
};

#endif // FILEOUTPUT_H
