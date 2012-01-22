#ifndef FILEOUTPUT_H
#define FILEOUTPUT_H

#include "baselmmoutput.h"

class QFile;

class FileOutput : public BaseLmmOutput
{
	Q_OBJECT
public:
	explicit FileOutput(QObject *parent = 0);
	int start();
	int stop();
	virtual int output();
signals:
	
public slots:
private:
	QFile *file;
};

#endif // FILEOUTPUT_H
