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
	BaseRtspSession(BaseRtspServer *parent);
	~BaseRtspSession();
	int setup(bool mcast, int dPort, int cPort, QString streamName);
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
	explicit BaseRtspServer(QObject *parent = 0, int port = 554);
	QString getMulticastAddress(const QString &streamName);
	int getMulticastPort(QString streamName);
	int setEnabled(bool val);
	QString getMulticastAddressBase();
	void setMulticastAddressBase(const QString &addr);
	void addStream(const QString streamName, bool multicast, RtpTransmitter *rtp, int port = 0, const QString &mcastAddress = "");

private slots:
	void newRtspConnection();
	void clientDisconnected(QObject*obj);
	void clientError(QObject*);
	void clientDataReady(QObject*obj);

protected:
	friend class BaseRtspSession;

	bool isMulticast(QString streamName); //protected
	RtpTransmitter * getSessionTransmitter(const QString &streamName); //protected
	void closeSession(QString sessionId);

private:
	struct StreamDescription {
		QString streamUrlSuffix;
		bool multicast;
		RtpTransmitter *rtp;
		int port;
		QString multicastAddr;
	};

	QTcpServer *server;
	QSignalMapper *mapperDis, *mapperErr, *mapperRead;
	QMap<QTcpSocket *, QString> msgbuffer;
	QMap<QString, BaseRtspSession *> sessions;
	QString currentPeerIp;
	QString lastUserAgent;
	QMap<QString, QString> currentCmdFields;
	bool enabled;
	QString multicastAddressBase;
	QHostAddress myIpAddr;
	QHostAddress myNetmask;
	QHash<QString, StreamDescription> streamDescriptions;

	QStringList createRtspErrorResponse(int errcode, QString lsep);
	QStringList createDescribeResponse(int cseq, QString url, QString lsep);
	QStringList handleRtspMessage(QString mes, QString lsep);
	void sendRtspMessage(QTcpSocket *sock, const QByteArray &mes);
	void sendRtspMessage(QTcpSocket *sock, QStringList &lines, const QString &lsep);
	QStringList createSdp(QString url);
	QString detectLineSeperator(QString mes);
	QString getField(const QStringList lines, QString desc);
	BaseRtspSession * findMulticastSession(QString streamName);
	bool isSessionMulticast(QString sid);

	/* command handling */
	QStringList handleCommandOptions(QStringList lines, QString lsep);
	QStringList handleCommandDescribe(QStringList lines, QString lsep);
	QStringList handleCommandSetup(QStringList lines, QString lsep);
	QStringList handleCommandPlay(QStringList lines, QString lsep);
	QStringList handleCommandTeardown(QStringList lines, QString lsep);
	QStringList handleCommandGetParameter(QStringList lines, QString lsep);

	uint getSessionBaseTimestamp(QString sid);
	uint getSessionBaseSequence(QString sid);
};

#endif // BASERTSPSERVER_H
