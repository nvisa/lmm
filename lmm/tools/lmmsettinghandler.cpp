#include "lmmsettinghandler.h"

#include "baseplayer.h"

LmmSettingHandler::LmmSettingHandler(QObject *parent) :
	BaseSettingHandler("lmm_settings", parent)
{
}

QVariant LmmSettingHandler::get(QString setting)
{
	if (equals("lmm_settings.stats.thread_status")) {
		BasePlayer *pl = (BasePlayer *)getTarget("BasePlayer");
		QList<LmmThread *> threads = pl->getThreads();
		QStringList list;
		foreach(LmmThread *t, threads) {
			list.append(QString("%1: %2 %3").arg(t->threadName()).arg((int)t->getStatus()).arg(t->elapsed()));
		}
		return list;
	}
	if (equals("lmm_settings.stats.buffer_status")) {
			BasePlayer *pl = (BasePlayer *)getTarget("BasePlayer");
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
	return QVariant();
}

int LmmSettingHandler::set(QString setting, QVariant value)
{
	return 0;
}

bool LmmSettingHandler::shouldCache(QString setting)
{
	if (starts("lmm_settings.stats"))
		return false;
	return BaseSettingHandler::shouldCache(setting);
}
