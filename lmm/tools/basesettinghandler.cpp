#include "basesettinghandler.h"
#include "baselmmelement.h"
#include "debug.h"

#include <QFile>
#include <QStringList>

#include <errno.h>
#include <unistd.h> /* for sync() */

QHash<QString, BaseSettingHandler *> BaseSettingHandler::handlers;
QSettings BaseSettingHandler::changed("/etc/vksystem/settings_incr.ini", QSettings::IniFormat);
QHash<QString, QList<BaseLmmElement *> > BaseSettingHandler::elementHandlers;
QHash<QString, QList<SettingHandlerInterface *> > BaseSettingHandler::generalHandlers;

BaseSettingHandler::BaseSettingHandler(QString key, QObject *parent) :
	QObject(parent)
{
	handlers.insert(key, this);
}

static int removeIndex(QStringList &l, QString &str)
{
	l.removeFirst();
	bool numeric = false;
	int ind = l[1].toInt(&numeric);
	if (numeric)
		l.removeFirst();
	str = l.join(".");
	return ind;
}

QVariant BaseSettingHandler::getSetting(const QString &setting)
{
	QStringList l = setting.split(".");
	if (l.size() < 2)
		return QVariant();
	const QString &base = l[0];
	if (elementHandlers.contains(base)) {
		QString str;
		int ind = removeIndex(l, str);
		const QList<BaseLmmElement *> &list = elementHandlers[base];
		if (list.size() > ind)
			return list[ind]->getSetting(str);
		return QVariant();
	} else if (generalHandlers.contains(base)) {
		QString str;
		int ind = removeIndex(l, str);
		const QList<SettingHandlerInterface *> &list = generalHandlers[base];
		if (list.size() > ind)
			return list[ind]->getSetting(str);
		return QVariant();
	} else if (handlers.contains(base))
		return handlers[base]->get(setting);
	return QVariant("*** error: No such handler ***");
}

QVariant BaseSettingHandler::getCache(const QString &setting)
{
	if (changed.contains(setting))
		return changed.value(setting);
	return QVariant();
}

QString BaseSettingHandler::getSettingString(QString setting)
{
	QVariant var = getSetting(setting);
	if (var.type() == QVariant::StringList) {
		QString str;
		QStringList l = var.toStringList();
		foreach (const QString &s, l)
			str.append(s).append("\n");
		return str;
	}
	return var.toString();
}

int BaseSettingHandler::setSetting(const QString &setting, const QVariant &value)
{
	int err = 0;
	/* '__base' space is for us */
	if (setting.startsWith("__base")) {
		if (equals("__base.sync")) {
			changed.sync();
			::sync();
		}
		return 0;
	}

	/* We check element handlers first */
	QStringList l = setting.split(".");
	const QString &base = l.first();
	if (elementHandlers.contains(base)) {
		QString str;
		int ind = removeIndex(l, str);
		const QList<BaseLmmElement *> &list = elementHandlers[base];
		if (list.size() > ind)
			err = list[ind]->setSetting(setting, value);
		else
			err = -EINVAL;
	} else if (generalHandlers.contains(base)) {
		QString str;
		int ind = removeIndex(l, str);
		const QList<SettingHandlerInterface *> &list = generalHandlers[base];
		if (list.size() > ind)
			err = list[ind]->setSetting(setting, value);
		else
			err = -EINVAL;
	} else if (handlers.contains(base)) {
		err = handlers[base]->set(setting, value);
	} else
		/* no element handlers registered for this space */
		return -ENOENT;

	/* not an error, just a flag for not saving ;) */
	if (err == -EROFS)
		return 0;
	if (err)
		return err;
	changed.setValue(setting, value);
	return 0;
}

int BaseSettingHandler::readIncrementalSettings(const QString &handler)
{
	QStringList keys = changed.childKeys();
	if (!handler.isEmpty()) {
		QStringList filtered;
		for (int i = 0; i < keys.size(); i++)
			if (keys[i].split(".").first() == handler)
				filtered << keys[i];
		keys = filtered;
	}
	QStringList cachedOnes;
	foreach (const QString key, keys) {
		QString last = key.split(".").last();
		if (last.startsWith("_"))
			cachedOnes  << key;
		else
			setSetting(key, changed.value(key));
	}
	foreach (const QString key, cachedOnes)
		setSetting(key, changed.value(key));
	return 0;
}

QHash<QString, QVariant> BaseSettingHandler::getSubSettings(const QString &groupName)
{
	QHash<QString, QVariant> sets;
	changed.beginGroup(groupName);
	const QStringList &keys = changed.childKeys();
	foreach (const QString &key, keys)
		sets.insert(key, changed.value(key));
	changed.endGroup();
	return sets;
}

void BaseSettingHandler::addHandler(const QString &base, BaseLmmElement *el)
{
	checkElementIndex(base, el);
}

void BaseSettingHandler::addHandler(const QString &base, SettingHandlerInterface *h)
{
	checkGeneralIndex(base, h);
}

int BaseSettingHandler::checkElementIndex(const QString &base, BaseLmmElement *el)
{
	if (!elementHandlers.contains(base))
		elementHandlers[base] << el;
	else if (!elementHandlers[base].contains(el))
		elementHandlers[base] << el;
	return elementHandlers[base].indexOf(el);
}

int BaseSettingHandler::checkGeneralIndex(const QString &base, SettingHandlerInterface *el)
{
	if (!generalHandlers.contains(base))
		generalHandlers[base] << el;
	else if (!generalHandlers[base].contains(el))
		generalHandlers[base] << el;
	return generalHandlers[base].indexOf(el);
}
