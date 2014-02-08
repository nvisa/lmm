#ifndef BASERTSPSERVER_H
#define BASERTSPSERVER_H

#include <QMap>
#include <QObject>
#include <QStringList>

#include <lmm/lmmcommon.h>

class QTcpServer;
class QTcpSocket;
class QSignalMapper;
class BaseRtspSession;

struct RtspSessionParameters {
	QString transportString;
	QString sessionId;
	bool multicast;
	QString controlUrl;
	int sourceDataPort;
	int sourceControlPort;
	int dataPort;
	int controlPort;
	QString peerIp;
	QString streamIp;
	QString streamName;
	uint ssrc;
};

class BaseRtspServer : public QObject
{
	Q_OBJECT
public:
	explicit BaseRtspServer(QObject *parent = 0);
	RtspSessionParameters getSessionParameters(QString id);
	virtual Lmm::CodecType getSessionCodec(QString streamName) = 0;
	virtual bool isMulticast(QString streamName) = 0;
	virtual QString getMulticastAddress(QString streamName);
	virtual int getMulticastPort(QString streamName);
	virtual int setEnabled(bool val);
	int getSessionTimeoutValue(QString id);
signals:
	void sessionSettedUp(QString);
	void sessionPlayed(QString);
	void sessionTearedDown(QString);
private slots:
	void newRtspConnection();
	void clientDisconnected(QObject*obj);
	void clientError(QObject*);
	void clientDataReady(QObject*obj);
private:
	QTcpServer *server;
	QSignalMapper *mapperDis, *mapperErr, *mapperRead;
	QMap<QTcpSocket *, QString> msgbuffer;
	QMap<QString, BaseRtspSession *> sessions;
	QString currentPeerIp;
protected:
	virtual QStringList createRtspErrorResponse(int errcode, QString lsep);
	virtual QStringList createDescribeResponse(int cseq, QString url, QString lsep);
	virtual QStringList handleRtspMessage(QString mes, QString lsep);
	virtual void sendRtspMessage(QTcpSocket *sock, const QByteArray &mes);
	virtual void sendRtspMessage(QTcpSocket *sock, QStringList &lines, const QString &lsep);
	virtual QStringList createSdp(QString url);
	virtual QString detectLineSeperator(QString mes);
	virtual QString getField(const QStringList lines, QString desc);
	virtual BaseRtspSession * findMulticastSession(QString streamName);
	virtual bool isSessionMulticast(QString sid);

	/* command handling */
	virtual QStringList handleCommandOptions(QStringList lines, QString lsep);
	virtual QStringList handleCommandDescribe(QStringList lines, QString lsep);
	virtual QStringList handleCommandSetup(QStringList lines, QString lsep);
	virtual QStringList handleCommandPlay(QStringList lines, QString lsep);
	virtual QStringList handleCommandTeardown(QStringList lines, QString lsep);
	virtual QStringList handleCommandGetParameter(QStringList lines, QString lsep);

	/* extra setup by inherited classes */
	virtual int sessionSetupExtra(QString) { return 0; }
	virtual int sessionPlayExtra(QString) { return 0; }
	virtual int sessionTeardownExtra(QString) { return 0; }

	virtual uint getSessionBaseTimestamp(QString sid) = 0;
	virtual uint getSessionBaseSequence(QString sid) = 0;

	void closeSession(QString sessionId);

	QString lastUserAgent;
	QMap<QString, QString> currentCmdFields;
	bool enabled;
};

#endif // BASERTSPSERVER_H
