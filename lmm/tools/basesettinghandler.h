#ifndef BASESETTINGHANDLER_H
#define BASESETTINGHANDLER_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QSettings>
#include <QStringList>

class BaseLmmElement;

#define strcontains(__str) setting.contains(__str, Qt::CaseInsensitive)
#define equals(__str) !setting.compare(__str, Qt::CaseInsensitive)
#define starts(__str) setting.startsWith(__str, Qt::CaseInsensitive)
#define ends(__str) setting.endsWith(__str, Qt::CaseInsensitive)

class SettingHandlerInterface
{
public:
	virtual QVariant getSetting(const QString &setting) = 0;
	virtual int setSetting(const QString &setting, const QVariant &value) = 0;
};

class BaseSettingHandler : public QObject
{
	Q_OBJECT
public:
	static QVariant getSetting(const QString &setting);
	static QVariant getCache(const QString &setting);
	static QString getSettingString(QString setting);
	static int setSetting(const QString &setting, const QVariant &value);
	static int readIncrementalSettings(const QString &handler = "");
	static QHash<QString, QVariant> getSubSettings(const QString &groupName);
	static void addHandler(const QString &base, BaseLmmElement *);
	static void addHandler(const QString &base, SettingHandlerInterface *);
protected:
	explicit BaseSettingHandler(QString key, QObject *parent = NULL);
	virtual QVariant get(const QString &setting) = 0;
	virtual int set(const QString &setting, const QVariant &value) = 0;
	static int checkElementIndex(const QString &base, BaseLmmElement *el);
	static int checkGeneralIndex(const QString &base, SettingHandlerInterface *el);

	static QHash<QString, BaseSettingHandler *> handlers;
	static QHash<QString, QList<SettingHandlerInterface *> > generalHandlers;
	static QHash<QString, QList<BaseLmmElement *> > elementHandlers;
	static QSettings changed;
};

#endif // BASESETTINGHANDLER_H
