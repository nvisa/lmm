#ifndef LMMSETTINGHANDLER_H
#define LMMSETTINGHANDLER_H

#include <lmm/tools/basesettinghandler.h>

class BasePlayer;

class LmmSettingHandler : public BaseSettingHandler
{
	Q_OBJECT
public:
	explicit LmmSettingHandler(QObject *parent = 0);
protected:
	QVariant get(const QString &setting);
	int set(const QString &setting, const QVariant &value);
	QVariant getPlayerSetting(QString setting, BasePlayer *pl);
};

#endif // LMMSETTINGHANDLER_H
