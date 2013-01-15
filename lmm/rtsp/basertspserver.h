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
};

class BaseRtspServer : public QObject
{
	Q_OBJECT
public:
	enum sessionType {
		H264_UNICAST,
		H264_MULTICAST,
		H264_LOWRES_UNICAST,
		H264_LOWRES_MULTICAST,
		MJPEG_UNICAST,
		MJPEG_MULTICAST
	};

	explicit BaseRtspServer(QObject *parent = 0);
	RtspSessionParameters getSessionParameters(QString url);
	static sessionType getSessionType(QString streamName);
	static Lmm::CodecType getSessionCodec(sessionType type);
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
	virtual QStringList createRtspErrorResponse(int errcode);
	virtual QStringList createDescribeResponse(int cseq, QString url, QString lsep);
	virtual QStringList handleRtspMessage(QString mes, QString lsep);
	virtual void sendRtspMessage(QTcpSocket *sock, const QByteArray &mes);
	virtual void sendRtspMessage(QTcpSocket *sock, QStringList &lines, const QString &lsep);
	virtual QStringList createSdp(QString url);
	virtual QString detectLineSeperator(QString mes);

	/* command handling */
	virtual QStringList handleCommandOptions(QStringList lines, QString lsep);
	virtual QStringList handleCommandDescribe(QStringList lines, QString lsep);
	virtual QStringList handleCommandSetup(QStringList lines, QString lsep);
	virtual QStringList handleCommandPlay(QStringList lines, QString lsep);
	virtual QStringList handleCommandTeardown(QStringList lines, QString lsep);
};

#endif // BASERTSPSERVER_H
