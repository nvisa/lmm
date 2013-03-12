#include "systeminfo.h"

#include "debug.h"

#include <QFile>
#include <QStringList>
#include <QTime>

SystemInfo SystemInfo::inst;

SystemInfo::SystemInfo(QObject *parent) :
	QObject(parent)
{
	meminfo = NULL;
	memtime.start();
}

int SystemInfo::getProcFreeMemInfo()
{
	if (memtime.elapsed() < 1000)
		return lastMemFree;
	if (!meminfo) {
		meminfo = new QFile("/proc/meminfo");
		meminfo->open(QIODevice::ReadOnly | QIODevice::Unbuffered);
	} else
		meminfo->seek(0);
	QStringList lines = QString::fromUtf8(meminfo->readAll()).split("\n");
	int free = 0;
	foreach (const QString line, lines) {
		/*
		 * 2 fields are important, MemFree and Cached
		 */
		if (line.startsWith("MemFree:"))
			free += line.split(" ", QString::SkipEmptyParts)[1].toInt();
		else if (line.startsWith("Cached:")) {
			free += line.split(" ", QString::SkipEmptyParts)[1].toInt();
			break;
		}
	}
	memtime.restart();
	/* prevent false measurements */
	if (free)
		lastMemFree = free;
	return lastMemFree;
}

int SystemInfo::getFreeMemory()
{
	return inst.getProcFreeMemInfo();
}
