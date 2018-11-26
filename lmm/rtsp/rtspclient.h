#ifndef RTSPCLIENT_H
#define RTSPCLIENT_H

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QHostAddress>
#include <QElapsedTimer>

class QTimer;
class QTcpSocket;
class RtpReceiver;

class RtspClient : public QObject
{
	Q_OBJECT
public:
	enum ConnectionState {
		UNKNOWN,
		OPTIONS_WAIT,
		DESCRIBE_WAIT,
		SETUP_WAIT,
		PLAY_WAIT,
		DESCRIBED,
		SETTEDUP,
		PLAYED,
		STREAMING
	};
	enum DeviceStatus {
		ALIVE,
		DEAD
	};
	explicit RtspClient(QObject *parent = 0);

	int setServerUrl(const QString &url);
	int getOptions();
	int getOptionsASync();
	int describeUrl();
	int describeUrlASync();
	int setupUrl();
	int setupUrlASync();
	int playSession(const QString &id);
	int playSessionASync(const QString &id);
	int teardownSession(const QString &id);
	int teardownSessionASync(const QString &id);
	int keepAlive(const QString &id);
	int keepAliveASync(const QString &id);
	RtpReceiver * getSessionRtp(const QString &id);
	void playASync();
	void teardownASync();

	void setDefaultRtpTrack(RtpReceiver *rtp);
	RtpReceiver * getTrackReceiver(const QString &name);
	void addSetupTrack(const QString &name, RtpReceiver *rtp);
	void clearSetupTracks();

	QStringList settedUpSessions() { return setupedSessions.keys(); }
	QStringList playingSessions() { return playedSessions.keys(); }

	DeviceStatus getDeviceStatus() { return devstatus; }
	ConnectionState getConnectionState() { return state; }

	void setAuthCredentials(const QString &username, const QString &password);

	void setMoxaHacks(bool moxaHacks);
protected slots:
	void timeout();
	void aSyncDataReady();
	void aSyncConnected();
	void aSyncDisConnected();
	void aSyncError(QAbstractSocket::SocketError);

protected:
	int getCSeq();
	void addAuthHeaders(QStringList &lines, const QString &method);
	int waitResponse(const QStringList &lines, QHash<QString, QString> &resp);
	bool readResponse(const QString &str, QHash<QString, QString> &resp);
	int setupTrack(const QString &controlUrl, const QString &connInfo, RtpReceiver *rtp);
	int setupTrackASync(const QString &controlUrl, const QString &connInfo, RtpReceiver *rtp);
	bool addFromDescribe(const QString &track);

	/* parsers */
	int parseOptionsResponse(const QHash<QString, QString> &resp);
	int parseDescribeResponse(const QHash<QString, QString> &resp);
	int parseSetupResponse(const QHash<QString, QString> &resp, RtpReceiver *rtp, QPair<int, int> p);
	int parseKeepAliveResponse(const QHash<QString, QString> &resp, const QString &id);
	int parsePlayResponse(const QHash<QString, QString> &resp, const QString &id);
	int parseTeardownResponse(const QHash<QString, QString> &resp, const QString &id);

	void closeAll();

	QString serverUrl;
	int cseq;
	QString msgbuffer;
	QTimer *timer;
	QHash<QString, QString> currentResp;
	bool asyncPlay;
	QString realm;
	QString nonce;
	QString user;
	QString pass;
	bool doMoxaHacks;

	struct ServerInfo
	{
		QStringList options;
	};
	ServerInfo serverInfo;
	struct RtspSession {
		QString id;
		QHash<QString, QString> pars;
		QElapsedTimer rtspTimeout;
		RtpReceiver *rtp;
	};
	struct TrackDefinition {
		QString name;
		QString type;
		QString controlUrl;
		QString m;
		QString rtpmap;
		QString connection;
	};

	QHash<QString, QList<TrackDefinition> > serverDescriptions;
	QHash<QString, RtspSession> setupedSessions;
	QHash<QString, RtspSession> playedSessions;
	QHash<QString, RtpReceiver *> trackReceivers;
	ConnectionState state;
	QTcpSocket *asyncsock;
	DeviceStatus devstatus;
	class CSeqRequest {
	public:
		CSeqRequest()
		{
			rtp = NULL;
		}

		CSeqRequest(QString s)
		{
			type = s;
		}

		QString type;
		RtpReceiver *rtp;
		QPair<int, int> p;
		QString id;
	};
	QHash<int, CSeqRequest> cseqRequests;
};

#endif // RTSPCLIENT_H
