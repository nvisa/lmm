#include "lmmsettinghandler.h"

#include "baseplayer.h"

#include <lmm/debug.h>
#include <lmm/streamtime.h>
#include <lmm/tools/cpuload.h>
#include <lmm/tools/systeminfo.h>
#include <lmm/tools/unittimestat.h>

LmmSettingHandler::LmmSettingHandler(QObject *parent) :
	BaseSettingHandler("lmm_settings", parent)
{
}

QVariant LmmSettingHandler::get(QString setting)
{
	if (starts("lmm_settings.players.")) {
		int index = setting.split(".")[2].toInt();
		BasePlayer *pl = (BasePlayer *)getTarget(QString("BasePlayer%1").arg(index));
		if (pl)
			return getPlayerSetting(setting, pl);
	}
	if (equals("lmm_settings.stats.thread_status")) {
		BasePlayer *pl = (BasePlayer *)getTarget("BasePlayer0");
		if (!pl)
			return QVariant();
		QList<LmmThread *> threads = pl->getThreads();
		QStringList list;
		foreach(LmmThread *t, threads) {
			list.append(QString("%1: %2 %3").arg(t->threadName()).arg((int)t->getStatus()).arg(t->elapsed()));
		}
		return list;
	}
	if (equals("lmm_settings.stats.buffer_status")) {
		BasePlayer *pl = (BasePlayer *)getTarget("BasePlayer0");
		if (!pl)
			return QVariant();
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
					.arg(el->getInputBufferCount())
					.arg(el->getOutputBufferCount())
					.arg(el->getReceivedBufferCount())
					.arg(el->getSentBufferCount())
					.arg(el->getInputSemCount(0))
					.arg(el->getOutputSemCount(0))
					;
			list.append(s);
		}
		return list;
	}
	if (equals("lmm_settings.stats.elements.fps")) {
		BasePlayer *pl = (BasePlayer *)getTarget("BasePlayer0");
		if (!pl)
			return QVariant();
		QList<BaseLmmElement *> elements = pl->getElements();
		QStringList list;
		for (int i = 0; i < elements.size(); i++) {
			BaseLmmElement *el = elements[i];
			QString s = QString("%1: %2").arg(el->metaObject()->className()).arg(el->getFps());
			list.append(s);
		}
		return list;
	}
	if (equals("lmm_settings.stats.elements.process_time")) {
		BasePlayer *pl = (BasePlayer *)getTarget("BasePlayer0");
		if (!pl)
			return QVariant();
		QList<BaseLmmElement *> elements = pl->getElements();
		QStringList list;
		for (int i = 0; i < elements.size(); i++) {
			BaseLmmElement *el = elements[i];
			UnitTimeStat *stat = el->getProcessTimeStat();
			QString s = QString("%1: %2,%3,%4").arg(el->metaObject()->className())
					.arg(stat->min).arg(stat->max).arg(stat->avgTime);
			list.append(s);
		}
		return list;
	}
	if (equals("lmm_settings.stats.extra_debug_info")) {
		BasePlayer *pl = (BasePlayer *)getTarget("BasePlayer0");
		if (!pl)
			return QVariant();
		QList<BaseLmmElement *> elements = pl->getElements();
		QStringList list;
		for (int i = 0; i < elements.size(); i++) {
			BaseLmmElement *el = elements[i];
			QList<QVariant> list2 = el->extraDebugInfo();
			QString s = QString("%1:extra").arg(el->metaObject()->className());
			foreach (const QVariant &var, list2) {
				if (var.canConvert(QVariant::StringList))
					s.append(QString(",%1").arg(var.toStringList().join(";")));
				else
					s.append(QString(",%1").arg(var.toString()));
			}
			list.append(s);
		}
		return list;
	}
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
	if (equals("lmm_settings.stats.prog_uptime")) {
		BasePlayer *pl = (BasePlayer *)getTarget("BasePlayer");
		if (!pl)
			return QVariant();
		return pl->getStreamTime()->getElapsedWallTime();
	}
	return QVariant();
}

int LmmSettingHandler::set(QString setting, QVariant value)
{
	return 0;
}

bool LmmSettingHandler::shouldCache(QString setting)
{
	return false;
	if (starts("lmm_settings.stats"))
		return false;
	return BaseSettingHandler::shouldCache(setting);
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
					.arg(el->getInputBufferCount())
					.arg(el->getOutputBufferCount())
					.arg(el->getReceivedBufferCount())
					.arg(el->getSentBufferCount())
					.arg(el->getInputSemCount(0))
					.arg(el->getOutputSemCount(0))
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
			QString s = QString("%1: %2: \n").arg(el->metaObject()->className()).arg(el->getFps());
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
