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
	QVariant get(QString setting);
	int set(QString setting, QVariant value);
	bool shouldCache(QString setting);
	QVariant getPlayerSetting(QString setting, BasePlayer *pl);
};

#endif // LMMSETTINGHANDLER_H
