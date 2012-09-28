#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include <QObject>
#include <QTime>

class QFile;

class SystemInfo : public QObject
{
	Q_OBJECT
public:
	static int getFreeMemory();
signals:
	
public slots:
private:
	explicit SystemInfo(QObject *parent = 0);
	int getProcFreeMemInfo();

	static SystemInfo inst;
	QFile *meminfo;
	QTime memtime;
	int lastMemFree;
};

#endif // SYSTEMINFO_H
