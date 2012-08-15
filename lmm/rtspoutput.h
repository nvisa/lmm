#ifndef RTSPOUTPUT_H
#define RTSPOUTPUT_H

#include "baselmmoutput.h"
#include "rawbuffer.h"

#include <QList>
#include <QMap>

class RtpStreamer;
class QTcpServer;
class QTcpSocket;
class QSignalMapper;
class QEventLoop;

class RtspOutput : public BaseLmmOutput
{
	Q_OBJECT
public:
	explicit RtspOutput(QObject *parent = 0);
	int start();
	int stop();
	int outputBuffer(RawBuffer buf);

	int getOutputBufferCount();
signals:
	
private slots:
	void newRtspConnection();
	void clientDisconnected(QObject*obj);
	void clientError(QObject*);
	void clientDataReady(QObject*obj);

	/* vlc slots */
	void connectedToVlc();
	void vlcDataReady();
private:
	RtpStreamer *gstRtp;
	QList<QTcpSocket *> clients;
	QTcpServer *server;
	QSignalMapper *mapperDis, *mapperErr, *mapperRead;
	QMap<QTcpSocket *, QString> msgbuffer;

	/* vlc variables */
	QTcpSocket *fwdSock;
	QEventLoop *vlcWait;
	QString lastVlcData;

	QStringList createDescribeResponse(int cseq, QString url, QString lsep);
	QStringList handleRtspMessage(QString mes, QString lsep, QString peerIp);
	void sendRtspMessage(QTcpSocket *sock, const QByteArray &mes);
	void sendRtspMessage(QTcpSocket *sock, const QStringList &lines, const QString &lsep);
	QStringList createSdp(QString url);
	QString forwardToVlc(const QString &mes);
};

#endif // RTSPOUTPUT_H
