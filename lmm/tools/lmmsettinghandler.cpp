#include "lmmsettinghandler.h"

#include "baseplayer.h"

#include <lmm/debug.h>
#include <lmm/streamtime.h>
#include <lmm/tools/cpuload.h>
#include <lmm/tools/systeminfo.h>
#include <lmm/tools/unittimestat.h>

#include <errno.h>

LmmSettingHandler::LmmSettingHandler(QObject *parent) :
	BaseSettingHandler("lmm_settings", parent)
{
}

QVariant LmmSettingHandler::get(const QString &setting)
{
	if (equals("lmm_settings.stats.cpu_load")) {
		return CpuLoad::getCpuLoad();
	}
	if (equals("lmm_settings.stats.avg_cpu_load")) {
		return CpuLoad::getAverageCpuLoad();
	}
	if (equals("lmm_settings.stats.free_memory")) {
		return SystemInfo::getFreeMemory();
	}
	if (equals("lmm_settings.stats.system_uptime")) {
		return SystemInfo::getUptime();
	}
	return QVariant();
}

int LmmSettingHandler::set(const QString &setting, const QVariant &value)
{
	if (starts("lmm_settings.fi")) {
		if (equals("lmm_settings.fi.add"))
			LmmCommon::addFaultInjection(value.toUInt());
		if (equals("lmm_settings.fi.remove"))
			LmmCommon::removeFaultInjection(value.toUInt());
		return -EROFS;
	}
	return 0;
}

const QStringList toStringList(const QList<QVariant> in)
{
	QStringList list;
	foreach (const QVariant &var, in) {
		if (var.canConvert(QVariant::StringList))
			list << var.toStringList().join(",");
		else
			list << var.toString();
	}
	return list;
}

QVariant LmmSettingHandler::getPlayerSetting(QString setting, BasePlayer *pl)
{
	if (ends(".stats.thread_status")) {
		QList<LmmThread *> threads = pl->getThreads();
		QStringList list;
		foreach(LmmThread *t, threads) {
			list.append(QString("%1: %2 %3").arg(t->threadName()).arg((int)t->getStatus()).arg(t->elapsed()));
		}
		return list;
	}
	if (ends(".buffer_status")) {
		QList<BaseLmmElement *> elements = pl->getElements();
		QStringList list;
		for (int i = 0; i < elements.size(); i++) {
			BaseLmmElement *el = elements[i];
			QString s = QString("%1: %2: \n"
								"\tinput_queue=%3 output_queue=%4\n"
								"\trecved=%5 sent=%6\n"
								"\tinput_sem=%7 output_sem=%8")
					.arg(i)
					.arg(el->metaObject()->className())
					.arg(el->getInputQueue(0)->getBufferCount())
					.arg(el->getOutputQueue(0)->getBufferCount())
					.arg(el->getInputQueue(0)->getReceivedCount())
					.arg(el->getOutputQueue(0)->getSentCount())
					.arg(el->getInputQueue(0)->getSemCount())
					.arg(el->getOutputQueue(0)->getSemCount())
					;
			list.append(s);
		}
		return list;
	}
	if (ends(".elements.fps")) {
		QList<BaseLmmElement *> elements = pl->getElements();
		QStringList list;
		for (int i = 0; i < elements.size(); i++) {
			BaseLmmElement *el = elements[i];
			QString s = QString("%1: %2: \n").arg(el->metaObject()->className()).arg(el->getOutputQueue(0)->getFps());
			list.append(s);
		}
		return list;
	}
	if (ends(".extra_debug_info")) {
		return toStringList(pl->extraDebugInfo());
	}
	if (ends(".elements.extra_debug_info")) {
		QList<BaseLmmElement *> elements = pl->getElements();
		QStringList list;
		for (int i = 0; i < elements.size(); i++) {
			BaseLmmElement *el = elements[i];
			QString s = QString("%1:extra").arg(el->metaObject()->className());
			list << s;
			list << toStringList(el->extraDebugInfo());
		}
		return list;
	}
	return QVariant();
}
