#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include <QObject>
#include <QElapsedTimer>

class QFile;

class SystemInfo : public QObject
{
	Q_OBJECT
public:
	static int getFreeMemory();
	static int getTVPVersion();
	static int getTVP5158Version(int addr);
	static int getADV7842Version();
	static int getTFP410DevId();
	static int getUptime();
signals:
	
public slots:
private:
	explicit SystemInfo(QObject *parent = 0);
	int getProcFreeMemInfo();

	static SystemInfo inst;
	QFile *meminfo;
	QElapsedTimer memtime;
	int lastMemFree;
};

#endif // SYSTEMINFO_H
