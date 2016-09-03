#ifndef SIMPLEZIP_H
#define SIMPLEZIP_H

#include <QHash>
#include <QObject>

struct SimpleZipPriv;

class SimpleZip : public QObject
{
	Q_OBJECT
public:
	explicit SimpleZip(QObject *parent = 0);
	~SimpleZip();
	QByteArray getArchive();
	bool saveArchive(const QString &filename);
	void addDirectory(const QString &dir);
	bool addFile(const QString &filename, const QByteArray &ba, const QString &comment = "");
signals:

public slots:
private:
	SimpleZipPriv *priv;
	//QHash<QString, QHash<QString, QByteArray>> files;
};

#endif // SIMPLEZIP_H
