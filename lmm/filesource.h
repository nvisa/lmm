#ifndef FILESOURCE_H
#define FILESOURCE_H

#include <QMap>
#include <QObject>

#include <lmm/baselmmelement.h>

class QFile;

class FileSource : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit FileSource(QObject *parent = 0);
	void setFilename(QString fn) { filename = fn; }
	int read(int size = 4096);
	virtual int start();
	virtual int stop();

signals:
	
public slots:
protected:
	QString filename;
	QFile *file;
	QMutex fileLock;

	int processBuffer(RawBuffer buf);
};

#endif // FILESOURCE_H
