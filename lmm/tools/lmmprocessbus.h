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
class LmmProcessBusImpl;

typedef qint32 sockType;

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

signals:
	void joined();

protected slots:
	void sockDataReady();
	void sockDisconnected();
	void connectedToServer();
protected:
	struct MessageData {
		qint32 sender;
		qint32 cmd;
		qint32 len;
		qint32 target;
		qint32 uid;
		qint32 version;
	};

	QByteArray createIntMessage(qint32 cmd, qint32 target, qint32 mes, qint32 uid = 0);
	QByteArray createStringMessage(qint32 cmd, qint32 target, const QString &mes, qint32 uid = 0);
	QByteArray createStringMessage1(qint32 cmd, qint32 target, const QString &mes, const QString &val1, qint32 uid = 0);
	QByteArray createStringMessage1(qint32 cmd, qint32 target, const QString &mes, const QVariant &val1, qint32 uid = 0);
	QByteArray createStringMessage1(qint32 cmd, qint32 target, const QString &mes, qint32 val1, qint32 uid = 0);
	QByteArray createVariantMessage(qint32 cmd, qint32 target, const QVariant &mes, qint32 uid = 0);
	QByteArray createCommandMessage(qint32 cmd, qint32 target, qint32 uid = 0);
	QByteArray processMessageCommon(QDataStream &in, qint32 sender, qint32 cmd, qint32 uid);
	virtual QByteArray processMessage(QDataStream &in, QLocalSocket *sock, const MessageData &msg);
	void processMessage(QLocalSocket *sock, QDataStream &in);
	void notifyWaiters(qint32 uid);
	bool waitMes(qint32 uid);

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

class LmmProcessBusServer : public LmmProcessBus
{
	Q_OBJECT
public:
	explicit LmmProcessBusServer(LmmPBusInterface *interface, QObject *parent = 0);

	/* server API */
	int setupBus();
	QStringList getConnectedProcesses();

protected slots:
	void clientConnectedToBus(sockType val);
	void clientDisconnectedFromBus(sockType val);

protected:
	QByteArray processMessage(QDataStream &in, QLocalSocket *sock, const MessageData &msg);

	LmmProcessBusImpl *bus;
	QList<int> uniqueIdList;
	QHash<qint32, QString> processNames;
	QHash<sockType, qint32> processIds;
};

class LmmProcessBusImpl : public QObject
{
	Q_OBJECT
public:
	LmmProcessBusImpl(QObject *parent = 0);

	int setup();
	int getProcessCount();

signals:
	void clientConnected(sockType);
	void clientDisconnected(sockType);

protected slots:
	void dataReady();
	void newConnection();
	void sockDisconnected();

protected:
	QLocalServer *server;
	QList<QLocalSocket *> connections;
};

#endif // LMMPROCESSBUS_H
