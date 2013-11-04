#ifndef BASESETTINGHANDLER_H
#define BASESETTINGHANDLER_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QStringList>

class BaseLmmElement;
class QXmlStreamWriter;

#define strcontains(__str) setting.contains(__str, Qt::CaseInsensitive)
#define equals(__str) !setting.compare(__str, Qt::CaseInsensitive)
#define starts(__str) setting.startsWith(__str, Qt::CaseInsensitive)
#define ends(__str) setting.endsWith(__str, Qt::CaseInsensitive)

class BaseSettingHandler : public QObject
{
	Q_OBJECT
public:
	static QVariant getSetting(QString setting);
	static QString getSettingString(QString setting);
	static int setSetting(QString setting, QVariant value);
	static int addTargets(QList<BaseLmmElement *> elements);
	static int addTarget(QObject * element);
	static QByteArray getAllInXml();
	static QStringList getKeyList();
	static bool isKeyReadable(QString key);
	static bool isKeyWriteable(QString key);
	static int importKeyList(QString filename);
	static int saveAllToDisk(QString filename);
	static int readAllFromDisk(QString filename);
protected:
	explicit BaseSettingHandler(QString key, QObject *parent = NULL);
	virtual QVariant get(QString setting) = 0;
	virtual int set(QString setting, QVariant value) = 0;
	virtual bool shouldCache(QString setting);
	QObject * getTarget(int target);
	QObject * getTarget(QString name);

	/* xml API */
	virtual int writeXml(QXmlStreamWriter *wr, QString section);
	void writeXmlList(QXmlStreamWriter *wr, const QStringList &list, QString main, QString el);

	static QMap<QString, int> settingKeys;
	static QMap<QString, QObject *> targetIndex;
	static QMap<QString, BaseSettingHandler *> handlers;
	QStringList targets;
	QMap<QString, QVariant> cache;
};

#endif // BASESETTINGHANDLER_H
