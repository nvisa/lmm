#ifndef LMMPROCESSBUS_H
#define LMMPROCESSBUS_H

#include <QHash>
#include <QObject>
#include <QVariant>
#include <QDataStream>
#include <QStringList>

class QEventLoop;
class QLocalServer;
class QLocalSocket;

class LmmPBusInterface
{
public:
	virtual QString getProcessName() = 0;
	virtual int getInt(const QString &fld) = 0;
	virtual QString getString(const QString &fld) = 0;
	virtual QVariant getVariant(const QString &fld) = 0;
	virtual int setInt(const QString &fld, qint32 val) = 0;
	virtual int setString(const QString &fld, const QString &val) = 0;
	virtual int setVariant(const QString &fld, const QVariant &val) = 0;
};

class LmmProcessBus : public QObject
{
	Q_OBJECT
public:
	explicit LmmProcessBus(LmmPBusInterface *interface, QObject *parent = 0);

	/* client API */
	int join();
	int waitForConnected();
	int waitForID();
	int lookUpId(const QString &pname);
	int getInt(qint32 target, const QString &fld);
	int setInt(qint32 target, const QString &fld, qint32 val);
	QString getString(qint32 target, const QString &fld);
	int setString(qint32 target, const QString &fld, const QString &val);
	QVariant getVariant(qint32 target, const QString &fld);
	int setVariant(qint32 target, const QString &fld, const QVariant &val);

	/* server API */
	int setupBus();
	QStringList getConnectedProcesses();
	int getProcessCount();

signals:
	void joined();

protected slots:
	void newConnection();
	void sockDataReady();
	void sockDisconnected();
	void connectedToServer();
	void sockError(int);
protected:
	QByteArray createIntMessage(qint32 cmd, qint32 target, qint32 mes, qint32 uid = 0);
	QByteArray createStringMessage(qint32 cmd, qint32 target, const QString &mes, qint32 uid = 0);
	QByteArray createStringMessage1(qint32 cmd, qint32 target, const QString &mes, const QString &val1, qint32 uid = 0);
	QByteArray createStringMessage1(qint32 cmd, qint32 target, const QString &mes, const QVariant &val1, qint32 uid = 0);
	QByteArray createStringMessage1(qint32 cmd, qint32 target, const QString &mes, qint32 val1, qint32 uid = 0);
	QByteArray createVariantMessage(qint32 cmd, qint32 target, const QVariant &mes, qint32 uid = 0);
	QByteArray createCommandMessage(qint32 cmd, qint32 target, qint32 uid = 0);
	QByteArray processMessageClient(QDataStream &in, QLocalSocket *sock, qint32 sender, qint32 cmd, qint32 target, qint32 uid);
	QByteArray processMessageServer(QDataStream &in, QLocalSocket *sock, qint32 sender, qint32 cmd, qint32 target, qint32 uid);
	QByteArray processMessageCommon(QDataStream &in, qint32 sender, qint32 cmd, qint32 uid);
	void processMessage(QLocalSocket *sock, QDataStream &in);
	void notifyWaiters(qint32 uid);
	bool waitMes(qint32 uid);

	/* server variables */
	QLocalServer *server;
	QList<QLocalSocket *> connections;
	QList<int> uniqueIdList;
	QHash<qint32, QString> processNames;
	QHash<QLocalSocket *, qint32> processIds;

	/* client variables */
	QLocalSocket *csock;
	QEventLoop *idWait;
	qint32 myid;

	/* common variables */
	LmmPBusInterface *iface;
	QHash<qint32, QEventLoop *> waiters;
	QHash<qint32, int> responseInt;
	QHash<qint32, QString> responseString;
	QHash<qint32, QVariant> responseVariant;
};

#endif // LMMPROCESSBUS_H
