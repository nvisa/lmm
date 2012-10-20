#ifndef RTSPOUTPUT_H
#define RTSPOUTPUT_H

#include "baselmmoutput.h"
#include "rawbuffer.h"
#include "lmmcommon.h"

#include <QList>
#include <QMap>
#include <QSemaphore>

class RtpStreamer;
class QTcpServer;
class QTcpSocket;
class QSignalMapper;
class QEventLoop;
class RtspSession;

class RtspOutput : public BaseLmmOutput
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
	explicit RtspOutput(QObject *parent = 0);
	int start();
	int stop();
	int outputBuffer(RawBuffer buf);
	int getLoopLatency();

	int getOutputBufferCount();
	static Lmm::CodecType getSessionCodec(sessionType type);
signals:
	void newSessionCreated(RtspOutput::sessionType);
private slots:
	void newRtspConnection();
	void clientDisconnected(QObject*obj);
	void clientError(QObject*);
	void clientDataReady(QObject*obj);
	void sessionNeedFlushing();

	/* vlc slots */
	void connectedToVlc();
	void vlcDataReady();
private:
	QMap<QString, RtspSession *> sessions;
	QList<QTcpSocket *> clients;
	QTcpServer *server;
	QSignalMapper *mapperDis, *mapperErr, *mapperRead;
	QMap<QTcpSocket *, QString> msgbuffer;

	/* vlc variables */
	QTcpSocket *fwdSock;
	QEventLoop *vlcWait;
	QString lastVlcData;

	QStringList createRtspErrorResponse(int errcode);
	QStringList createDescribeResponse(int cseq, QString url, QString lsep);
	QStringList handleRtspMessage(QString mes, QString lsep, QString peerIp);
	void sendRtspMessage(QTcpSocket *sock, const QByteArray &mes);
	void sendRtspMessage(QTcpSocket *sock, const QStringList &lines, const QString &lsep);
	QStringList createSdp(QString url);
	QString forwardToVlc(const QString &mes);
	bool canSetupMore(bool multicast);
	RtspSession * findSession(bool multicast, QString url = "", QString sessionId = "");
	sessionType getSessionType(QString streamName);
};

#endif // RTSPOUTPUT_H
