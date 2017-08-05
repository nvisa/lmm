#ifndef RTSPCLIENT_H
#define RTSPCLIENT_H

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QElapsedTimer>

class QTimer;
class RtpReceiver;

class RtspClient : public QObject
{
	Q_OBJECT
public:
	explicit RtspClient(QObject *parent = 0);

	int setServerUrl(const QString &url);
	int getOptions();
	int describeUrl();
	int setupUrl();
	int playSession(const QString &id);
	int teardownSession(const QString &id);
	int keepAlive(const QString &id);
	RtpReceiver * getSessionRtp(const QString &id);

	void addSetupTrack(const QString &name, RtpReceiver *rtp);
	void clearSetupTracks();

	QStringList settedUpSessions() { return setupedSessions.keys(); }
	QStringList playingSessions() { return playedSessions.keys(); }

protected slots:
	void timeout();

protected:
	int getCSeq();
	int waitResponse(const QStringList &lines, QHash<QString, QString> &resp);
	int setupTrack(const QString &controlUrl, const QString &connInfo, RtpReceiver *rtp);

	QString serverUrl;
	int cseq;
	QString msgbuffer;
	QTimer *timer;

	enum ConnectionState {
		UNKNOWN,
		DESCRIBED,
		SETTEDUP,
		PLAYED,
		STREAMING
	};
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
	QStringList setupTracks;
	ConnectionState state;
};

#endif // RTSPCLIENT_H
