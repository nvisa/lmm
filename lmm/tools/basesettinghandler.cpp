#include "basesettinghandler.h"

#include <QFile>
#include <QStringList>
#include <QXmlStreamWriter>

#include <lmm/debug.h>
#include <lmm/baselmmelement.h>

#include <errno.h>

#include <unistd.h> /* for sync() */

QMap<QString, int> BaseSettingHandler::settingKeys;
QMap<QString, BaseSettingHandler *> BaseSettingHandler::handlers;
QMap<QString, QObject *> BaseSettingHandler::targetIndex;
QSettings BaseSettingHandler::changed("/etc/vksystem/settings_incr.ini", QSettings::IniFormat);

BaseSettingHandler::BaseSettingHandler(QString key, QObject *parent) :
	QObject(parent)
{
	handlers.insert(key, this);
}

bool BaseSettingHandler::shouldCache(QString)
{
	return true;
}

int BaseSettingHandler::writeXml(QXmlStreamWriter *wr, QString section)
{
	return -ENOENT;
}

void BaseSettingHandler::writeXmlList(QXmlStreamWriter *wr, const QStringList &list, QString main, QString el)
{
	wr->writeStartElement(main);
	foreach (const QString l, list)
		wr->writeTextElement(el, l);
	wr->writeEndElement();
}

QObject * BaseSettingHandler::getTarget(int target)
{
	return targetIndex[targets[target]];
}

QObject *BaseSettingHandler::getTarget(QString name)
{
	if (!targetIndex.contains(name)) {
		mDebug("no such target '%s'", qPrintable(name));
		return NULL;
	}
	return targetIndex[name];
}

QVariant BaseSettingHandler::getSetting(QString setting)
{
	QStringList l = setting.split(".");
	if (handlers.contains(l.first())) {
		BaseSettingHandler *h = handlers[l.first()];
		if (h->cache.contains(setting))
			return h->cache[setting];
		QVariant s = h->get(setting);
		if (s.isValid() && h->shouldCache(setting))
			h->cache.insert(setting, s);
		return s;
	}
	return QVariant("*** error: No such handler ***");
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

int BaseSettingHandler::setSetting(QString setting, QVariant value)
{
	if (setting.startsWith("__base")) {
		if (equals("__base.sync")) {
			changed.sync();
			::sync();
		}
		return 0;
	}
	QStringList l = setting.split(".");
	if (!handlers.contains(l.first()))
		return -ENOENT;
	BaseSettingHandler *h = handlers[l.first()];
	fDebug("setting '%s'", qPrintable(setting));
	int err = h->set(setting, value);
	/* not an error, just a flag for not saving ;) */
	if (err == -EROFS)
		return 0;
	if (err)
		return err;
	changed.setValue(setting, value);
	h->cache.insert(setting, value);
	return 0;
}

int BaseSettingHandler::addTargets(QList<BaseLmmElement *> elements)
{
	foreach (BaseLmmElement *el, elements)
		addTarget(el);
	return 0;
}

int BaseSettingHandler::addTarget(QObject *element)
{
	fDebug("adding '%s':'%s'", qPrintable(element->objectName()), element->metaObject()->className());
	if (element->objectName().isEmpty())
		targetIndex.insert(element->metaObject()->className(), element);
	else
		targetIndex.insert(element->objectName(), element);
	return 0;
}

QByteArray BaseSettingHandler::getAllInXml()
{
	QByteArray ba;
	QXmlStreamWriter wr(&ba);
	wr.setAutoFormatting(true);
	wr.writeStartDocument();
	wr.writeStartElement("settings");

	QMapIterator<QString, BaseSettingHandler *> i(handlers);
	while (i.hasNext()) {
		i.next();
		i.value()->writeXml(&wr, i.key());
	}

	wr.writeEndElement();
	wr.writeEndDocument();
	return ba;
}

QStringList BaseSettingHandler::getKeyList()
{
	return settingKeys.keys();
}

bool BaseSettingHandler::isKeyReadable(QString key)
{
	if (!settingKeys.contains(key))
		return false;
	return (settingKeys[key] & 1);
}

bool BaseSettingHandler::isKeyWriteable(QString key)
{
	if (!settingKeys.contains(key))
		return false;
	return (settingKeys[key] & 2);
}

int BaseSettingHandler::importKeyList(QString filename)
{
	settingKeys.clear();
	QFile f(filename);
	if (!f.open(QIODevice::ReadOnly))
		return -ENOENT;
	QStringList lines = QString::fromUtf8(f.readAll()).split("\n");
	foreach (const QString &line, lines) {
		QStringList fields = line.split(" ", QString::SkipEmptyParts);
		if (fields.size() < 2)
			continue;
		QString s = fields[0].trimmed();
		QString st = fields[1].trimmed();
		QString cache;
		if (fields.size() > 2)
			cache = fields[2].trimmed();
		int curr = 0;
		if (st == "[r]")
			curr |= 1;
		else if (st == "[w]")
			curr |= 2;
		else if (st == "[rw]")
			curr |= 3;
		if (cache != "")
			curr |= 4;
		settingKeys[s] = curr;
	}
	return 0;
}

int BaseSettingHandler::saveAllToDisk(QString filename)
{
	QFile f(filename);
	if (!f.open(QIODevice::WriteOnly))
		return f.error() == QFile::PermissionsError ? -EPERM : -ENOENT;

	QMapIterator <QString, int> i(settingKeys);
	while (i.hasNext()) {
		i.next();
		fDebug("processing setting '%s'", qPrintable(i.key()));
		if ((i.value() & 1) == 0)
			continue;
		QString str;
		QVariant v = getSetting(i.key());
		if (v.type() == QVariant::String)
			str = getSetting(i.key()).toString();
		else if (v.type() == QVariant::StringList)
			str = getSetting(i.key()).toStringList().join(",");
		else
			str = getSetting(i.key()).toString();
		f.write(QString("%1=%2\n").arg(i.key()).arg(str).toUtf8());
	}
	f.close();
	return 0;
}

int BaseSettingHandler::readAllFromDisk(QString filename)
{
	QFile f(filename);
	if (!f.open(QIODevice::ReadOnly))
		return f.error() == QFile::PermissionsError ? -EPERM : -ENOENT;

	QMap<QString, QString> cached;
	QStringList lines = QString::fromUtf8(f.readAll()).split("\n");
	foreach (const QString &line, lines) {
		QStringList fields = line.split("=");
		if (fields.size() < 2) {
			fDebug("corrupted line '%s', skipping...", qPrintable(line));
			continue;
		}
		QString key = fields[0].trimmed();
		if (!settingKeys.contains(key)) {
			fDebug("no such key as '%s' in keylist, skipping...", qPrintable(key));
			continue;
		}
		if ((settingKeys[key] & 2) == 0) {
			fDebug("key '%s' is not writable, skipping...", qPrintable(key));
			continue;
		}
		if (settingKeys[key] & 4) {
			fDebug("key '%s' is cached, setting last.", qPrintable(key));
			cached.insert(key, fields[1].trimmed());
			continue;
		}
		setSetting(key, fields[1].trimmed());
	}
	fDebug("setting cached properties");
	QMapIterator<QString, QString> i(cached);
	while (i.hasNext()) {
		i.next();
		setSetting(i.key(), i.value());
	}
	f.close();

	return 0;
}

int BaseSettingHandler::readIncrementalSettings()
{
	QStringList keys = changed.allKeys();
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
