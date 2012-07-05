#ifndef DEBUGCLIENT_H
#define DEBUGCLIENT_H

#include <QObject>
#include <QAbstractSocket>
#include <QStringList>
#include <QList>

#include <lmm/debugserver.h>

class QTcpSocket;

class DebugClient : public QObject
{
	Q_OBJECT
public:
	enum Command {
		CMD_SYNC_TIME,
		CMD_APP_SPECIFIC,
	};

	explicit DebugClient(QObject *parent = 0);
	void connectToServer(QString ipAddr);

	bool isConnected();
	QString peerIp();

	int getElementCount();
	const QStringList getElementNames();
	int getInputBufferCount(int el);
	int getOutputBufferCount(int el);
	int getReceivedBufferCount(int el);
	int getSentBufferCount(int el);
	int getFps(int el);
	int getCustomStat(DebugServer::CustomStat stat);
	int getMinOutputTime(int el);
	int getMaxOutputTime(int el);
	int getAverageOutputTime(int el);

	QStringList getDebugMessages();
	QStringList getWarningMessages();

	int sendCommand(Command cmd, QVariant par);
signals:
	void connectedToDebugServer();
	void disconnectedFromDebugServer();
	void newApplicationMessage(QTcpSocket *client, const QString &cmd,
							   const QByteArray &data);
private slots:
	void timeout();
	void handleMessage(QTcpSocket *client,
					   const QString &cmd, const QByteArray &data);
	void connectedToServer();
	void disconnectedFromServer();
	void readServerMessage();
	void socketError(QAbstractSocket::SocketError err);
private:
	QTcpSocket *client;
	qint32 blocksize;

	QStringList names;
	QList<QList<int> > stats;
	QHash<int, int> customStats;

	QStringList debugMessages;
	QStringList warningMessages;
};

#endif // DEBUGCLIENT_H
