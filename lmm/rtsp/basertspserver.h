#ifndef BASERTSPSERVER_H
#define BASERTSPSERVER_H

#include <QMap>
#include <QObject>
#include <QStringList>
#include <QHostAddress>

#include <lmm/lmmcommon.h>
#include <lmm/baselmmelement.h>

class MyTime;
class QTcpServer;
class QTcpSocket;
class RtpChannel;
class QSignalMapper;
class BaseRtspServer;
class RtpTransmitter;

class BaseRtspSession : public QObject
{
	Q_OBJECT
public:
	BaseRtspSession(const QString &iface, BaseRtspServer *parent);
	~BaseRtspSession();
	int setup(bool mcast, int dPort, int cPort, const QString &streamName, const QString &media, const QString &incomingTransportString);
	int play();
	int teardown();
	QString rtpInfo();
	RtpChannel * getRtpChannel() { return rtpCh; }

	enum SessionState {
		SETUP,
		PLAY,
		TEARDOWN
	};
	SessionState state;
	QString transportString;
	QString sessionId;
	bool multicast;
	QString controlUrl;
	QString streamName;
	QString mediaName;
	int sourceDataPort;
	int sourceControlPort;
	int dataPort;
	int controlPort;
	QString peerIp;
	QString streamIp;
	int clientCount;
	uint ssrc;
	uint ttl;
	MyTime *timeout;
	int rtptime;
	int seq;
	bool rtspTimeoutEnabled;
	QList<BaseRtspSession *> siblings;
	bool rtpAvpTcp;
protected slots:
	void rtpGoodbyeRecved();
	void rtcpTimedOut();
private:
	QHostAddress myIpAddr;
	BaseRtspServer *server;
	RtpChannel *rtpCh;
	RtpTransmitter *rtp;
};

class BaseRtspServer : public QObject
{
	Q_OBJECT
public:
	enum Auth {
		AUTH_NONE,
		AUTH_SIMPLE,
		AUTH_DIGEST,
	};

	explicit BaseRtspServer(QObject *parent = 0, int port = 554);
	QString getMulticastAddress(const QString &streamName, const QString &media);
	int getMulticastPort(QString streamName, const QString &media);
	int setEnabled(bool val);
	QString getMulticastAddressBase(const QString &streamName, const QString &media);
	void setMulticastAddressBase(const QString &streamName, const QString &media, const QString &addr);
	void addStream(const QString streamName, bool multicast, int port = 0, const QString &mcastAddress = "");
	void addStream(const QString streamName, bool multicast, RtpTransmitter *rtp, int port = 0, const QString &mcastAddress = "");
	void addMedia2Stream(const QString &mediaName, const QString &streamName, bool multicast, RtpTransmitter *rtp, int port = 0, const QString &mcastAddress = "");
	bool hasStream(const QString &streamName);
	void addStreamParameter(const QString &streamName, const QString &mediaName, const QString &par, const QVariant &value);
	const QHash<QString, QVariant> getStreamParameters(const QString &streamName, const QString &mediaName);
	void setRtspAuthentication(Auth authMethod);
	QString getNetworkInterface() { return nwInterfaceName; }
	void setNetworkInterface(const QString &name) { nwInterfaceName = name; }
	void setRtspAuthenticationCredentials(const QString &username, const QString &password);

	/* session API */
	const QStringList getSessions();
	const QStringList getSessions(const QString &streamName);
	const BaseRtspSession * getSession(const QString &sid);
	void saveSessions(const QString &filename);
	int loadSessions(const QString &filename);

	int newRtpData(const char *data, int size, RtpChannel *ch);

signals:
	void newRtpTcpData(const QByteArray &ba, RtpChannel *ch);
private slots:
	void handleNewRtpTcpData(const QByteArray &ba, RtpChannel *ch);
	void newRtspConnection();
	void clientDisconnected(QObject*obj);
	void clientError(QObject*);
	void clientDataReady(QObject*obj);

protected:
	friend class BaseRtspSession;

	bool isMulticast(QString streamName, const QString &media); //protected
	RtpTransmitter * getSessionTransmitter(const QString &streamName, const QString &media); //protected
	void closeSession(QString sessionId);

private:
	struct StreamDescription {
		QString streamUrlSuffix;
		bool multicast;
		RtpTransmitter *rtp;
		int port;
		QString multicastAddr;
		QString multicastAddressBase;
		QHash<QString, QVariant> meta;
		QHash<QString, StreamDescription> media;
	};

	QTcpServer *server;
	QSignalMapper *mapperDis, *mapperErr, *mapperRead;
	QMap<QTcpSocket *, QString> msgbuffer;
	QMap<QString, BaseRtspSession *> sessions;
	QString currentPeerIp;
	QString lastUserAgent;
	QMap<QString, QString> currentCmdFields;
	bool enabled;
	QHostAddress myIpAddr;
	QHostAddress myNetmask;
	QHash<QString, StreamDescription> streamDescriptions;
	Auth auth;
	QMutex sessionLock;
	QString nwInterfaceName;
	QString authUsername;
	QString authPassword;
	QTcpSocket *currentSocket;
	QHash<RtpChannel *, QTcpSocket *> avpTcpMappings;

	QStringList createRtspErrorResponse(int errcode, QString lsep);
	QStringList createDescribeResponse(int cseq, QString url, QString lsep);
	QStringList handleRtspMessage(QString mes, QString lsep);
	void sendRtspMessage(QTcpSocket *sock, const QByteArray &mes);
	void sendRtspMessage(QTcpSocket *sock, QStringList &lines, const QString &lsep);
	QStringList createSdp(QString url);
	QString detectLineSeperator(QString mes);
	QString getField(const QStringList lines, QString desc);
	BaseRtspSession * findMulticastSession(const QString &streamName, const QString &media);
	bool isSessionMulticast(QString sid);
	const StreamDescription getStreamDesc(const QString &streamName, const QString &mediaName);

	/* command handling */
	QStringList handleCommandOptions(QStringList lines, QString lsep);
	QStringList handleCommandDescribe(QStringList lines, QString lsep);
	QStringList handleCommandSetup(QStringList lines, QString lsep);
	QStringList handleCommandPlay(QStringList lines, QString lsep);
	QStringList handleCommandTeardown(QStringList lines, QString lsep);
	QStringList handleCommandGetParameter(QStringList lines, QString lsep);
	QStringList handleCommandSetParameter(QStringList lines, QString lsep);

	uint getSessionBaseTimestamp(QString sid);
	uint getSessionBaseSequence(QString sid);
};

#endif // BASERTSPSERVER_H
