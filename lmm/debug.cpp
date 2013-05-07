#include "debug.h"

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

#include <QTextCodec>

#ifdef DEBUG_TIMING
QTime __debugTimer;
unsigned int __lastTime;
unsigned int __totalTime;
#endif

QStringList __dbg_classes;
QStringList __dbg_classes_info;
QStringList __dbg_classes_log;
QStringList __dbg_classes_logv;

void changeDebug(QString debug, int defaultLevel)
{
	QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
	QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#ifdef DEBUG_TIMING
	__debugTimer.start();
	__lastTime = __totalTime = 0;
#endif
	__dbg_classes.clear();
	__dbg_classes_info.clear();
	__dbg_classes_log.clear();
	__dbg_classes_logv.clear();
	QStringList debugees;
	__dbg_classes << "__na__";
	__dbg_classes_info << "__na__";
	__dbg_classes_log << "__na__";
	__dbg_classes_logv << "__na__";
	debugees = debug.split(",");
	foreach (const QString d, debugees) {
		if (d.isEmpty())
			continue;
		QString className;
		int level = defaultLevel;
		if (d.contains(":")) {
			QStringList pair = d.split(":");
			className = pair[0];
			level = pair[1].toInt();
		} else
			className = d;
		if (className == "*") {
			if (level > 0)
				__dbg_classes.clear();
			if (level > 1)
				__dbg_classes_info.clear();
			break;
		}
		if (level > 0)
			__dbg_classes << className;
		if (level > 1)
			__dbg_classes_info << className;
		if (level > 2)
			__dbg_classes_log << className;
		if (level > 3)
			__dbg_classes_logv << className;
	}
	if (__dbg_classes.size() > 1)
		__dbg_classes.removeFirst();
	if (__dbg_classes_info.size() > 1)
		__dbg_classes_info.removeFirst();
	if (__dbg_classes_log.size() > 2)
		__dbg_classes_log.removeFirst();
	if (__dbg_classes_logv.size() > 3)
		__dbg_classes_logv.removeFirst();
	if (__dbg_classes.size())
		qDebug() << "debug classes:" << __dbg_classes << __dbg_classes_info << __dbg_classes_log << __dbg_classes_logv;
}

void initDebug()
{
	QString env = QString(getenv("DEBUG"));
	changeDebug(env);
}

/* Obtain a backtrace and print it to stdout. */
void print_trace(void)
{
	void *array[10];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace (array, 10);
	strings = backtrace_symbols (array, size);

	printf ("Obtained %zd stack frames.\n", size);

	for (i = 0; i < size; i++)
		printf ("%s\n", strings[i]);

	free (strings);
}
