#include "rtspoutput.h"
#include "baselmmoutput.h"
#include "gstreamer/rtpstreamer.h"

#include <emdesk/debug.h>

#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalMapper>
#include <QStringList>
#include <QEventLoop>
#include <QUuid>
#include <QNetworkInterface>
#include <QHostAddress>

class RtspSession
{
public:
	RtspSession()
	{
		gstRtp = new RtpStreamer;
		state = TEARDOWN;
		/* Let's find our IP address */
		foreach (const QNetworkInterface iface, QNetworkInterface::allInterfaces()) {
			if (iface.name() != "eth0")
				continue;
			if (iface.addressEntries().size()) {
				myIpAddr = iface.addressEntries().at(0).ip();
				break;
			}
		}
	}
	int setup(bool mcast, int dPort, int cPort, QString clientIp)
	{
		if (mcast && state != TEARDOWN) {
			joinedIps << clientIp;
			return 0;
		}
		multicast = mcast;
		dataPort = dPort;
		controlPort = cPort;
		joinedIps << clientIp;

		if (multicast) {
			gstRtp->setDestinationIpAddress("224.1.1.1");
			transportString = QString("Transport: RTP/AVP;multicast;destination=224.1.1.1;port=%1-%2;ssrc=6335D514;mode=play")
					.arg(dataPort).arg(controlPort);
		} else {
			gstRtp->setDestinationIpAddress(joinedIps.first());
			transportString = QString("Transport: RTP/AVP/UDP;unicast;client_port=%1-%2;server_port=%3-%4;ssrc=6335D514;mode=play")
					.arg(dataPort).arg(controlPort).arg(gstRtp->getSourceDataPort()).arg(gstRtp->getSourceControlPort());
		}
		gstRtp->setDestinationDataPort(dataPort);
		gstRtp->setDestinationControlPort(controlPort);
		/* create session identifier */
		sessionId = QUuid::createUuid().toString().split("-")[4].remove("}");
		state = SETUP;
		return 0;
	}
	QString rtpInfo()
	{
		if (multicast)
			return QString("url=rtsp://%1/stream1m;seq=6666;rtptime=1578998879").arg(myIpAddr.toString());
		return QString("url=rtsp://%1/stream1;seq=6666;rtptime=1578998879").arg(myIpAddr.toString());
	}
	int play()
	{
		if (state != PLAY) {
			gstRtp->setRtpSequenceOffset(6666);
			gstRtp->setRtpTimestampOffset(1578998879);
			gstRtp->startPlayback();
			state = PLAY;
		}
		return 0;
	}
	int teardown(QString peerIp)
	{
		if (joinedIps.contains(peerIp))
			joinedIps.removeOne(peerIp);
		if (state == PLAY && joinedIps.size() == 0) {
			gstRtp->stopPlayback();
			state = TEARDOWN;
		}
		return 0;
	}
	int sendBuffer(RawBuffer buf)
	{
		if (!gstRtp->isRunning())
			return -1;
		gstRtp->sendBuffer(buf);
		return 0;
	}
	int getWaitingBufferCount()
	{
		return gstRtp->getWaitingBufferCount();
	}

public:
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
	QStringList joinedIps;
private:
	RtpStreamer *gstRtp;
	int dataPort;
	int controlPort;
	QHostAddress myIpAddr;
};

RtspOutput::RtspOutput(QObject *parent) :
	BaseLmmOutput(parent)
{
	server = new QTcpServer(this);
	if (!server->listen(QHostAddress::Any, 554))
		mDebug("unable to bind to tcp port 554");
	connect(server, SIGNAL(newConnection()), SLOT(newRtspConnection()));
	mapperDis = new QSignalMapper(this);
	mapperErr = new QSignalMapper(this);
	mapperRead = new QSignalMapper(this);
	connect(mapperDis, SIGNAL(mapped(QObject*)), SLOT(clientDisconnected(QObject*)));
	connect(mapperErr, SIGNAL(mapped(QObject*)), SLOT(clientError(QObject*)));
	connect(mapperRead, SIGNAL(mapped(QObject*)), SLOT(clientDataReady(QObject*)));

	fwdSock = new QTcpSocket(this);
	connect(fwdSock, SIGNAL(connected()), SLOT(connectedToVlc()));
	connect(fwdSock, SIGNAL(readyRead()), SLOT(vlcDataReady()));
	vlcWait = new QEventLoop(this);
}

int RtspOutput::start()
{
	return BaseLmmOutput::start();
}

int RtspOutput::stop()
{
	return BaseLmmOutput::stop();
}

int RtspOutput::outputBuffer(RawBuffer buf)
{
	mInfo("pushing new buffer with size %d", buf.size());
	foreach (RtspSession *s, sessions)
		s->sendBuffer(buf);
	return 0;
}

int RtspOutput::getOutputBufferCount()
{
	int cnt = 0;
	foreach (RtspSession *s, sessions)
		cnt += s->getWaitingBufferCount();
	return BaseLmmElement::getOutputBufferCount() + cnt;
}

void RtspOutput::newRtspConnection()
{
	QTcpSocket *sock = server->nextPendingConnection();
	connect(sock, SIGNAL(disconnected()), mapperDis, SLOT(map()));
	connect(sock, SIGNAL(error(QAbstractSocket::SocketError)), mapperErr, SLOT(map()));
	connect(sock, SIGNAL(readyRead()), mapperRead, SLOT(map()));
	mapperDis->setMapping(sock, sock);
	mapperErr->setMapping(sock, sock);
	mapperRead->setMapping(sock, sock);
	clients << sock;
	mDebug("client %s connected", qPrintable(sock->peerAddress().toString()));

	/* forward connection for vlc */
	//fwdSock->connectToHost(QHostAddress::LocalHost, 8080);
}

void RtspOutput::clientDisconnected(QObject *obj)
{
	QTcpSocket *sock = (QTcpSocket *)obj;
	//fwdSock->abort();
	sock->abort();
	sock->deleteLater();
	clients.removeOne(sock);
	qDebug() << "connection closed";
	foreach (RtspSession *s, sessions) {
		s->teardown(sock->peerAddress().toString());
	}
}

void RtspOutput::clientError(QObject *)
{
	qDebug() << "connection error";
}

void RtspOutput::clientDataReady(QObject *obj)
{
	QTcpSocket *sock = (QTcpSocket *)obj;
	/* RTSP protocol uses UTF-8 */
	QString str = QString::fromUtf8(sock->readAll());
	QString mes = msgbuffer[sock].append(str);
	QString lsep;
	if (mes.contains("\r\n")) {
		lsep = "\r\n";
		mDebug("detected lsep as \\r\\n");
	} else if (mes.contains("\n\r")) {
		lsep = "\n\r";
		mDebug("detected lsep as \\n\\r");
	} else if (mes.contains("\n")) {
		lsep = "\n";
		mDebug("detected lsep as \\n");
	} else if (mes.contains("\r")) {
		lsep = "\r";
		mDebug("detected lsep as \\r");
	}
	QString end = lsep + lsep;
	if (mes.contains(end)) {
		qDebug() << "new message from rtsp client: " << mes;
#if 1
		QStringList resp = handleRtspMessage(mes, lsep, sock->peerAddress().toString());
		msgbuffer[sock] = "";
		if (resp.size())
			sendRtspMessage(sock, resp, lsep);
#else
		msgbuffer[sock] = "";
		if (fwdSock->state() == QAbstractSocket::ConnectedState) {
			fwdSock->write(mes.toUtf8());
			vlcWait->exec();
			sendRtspMessage(sock, forwardToVlc(mes).toUtf8());
		} else {
			qDebug() << "vlc is not connected, waiting for connection";
			vlcWait->exec();
			sendRtspMessage(sock, forwardToVlc(mes).toUtf8());
		}
#endif
	} else
		msgbuffer[sock] = mes;
}

void RtspOutput::connectedToVlc()
{
	qDebug() << "connected to vlc";
	vlcWait->quit();
}

void RtspOutput::vlcDataReady()
{
	lastVlcData = QString::fromUtf8(fwdSock->readAll());
	qDebug() << "*** vlc message : ***" << lastVlcData;
	vlcWait->quit();
}

QStringList RtspOutput::createRtspErrorResponse(int errcode)
{
	QStringList resp;
	QString errString;
	if (errcode == 400)
		errString = "Bad Request";
	else if (errcode == 403)
		errString = "Forbidden";
	else if (errcode == 404)
		errString = "Not Found";
	else if (errcode == 406)
		errString = "Not Acceptable";
	else if (errcode == 451)
		errString = "Invalid parameter";
	else if (errcode == 453)
		errString = "Not Enough Bandwidth";
	resp << QString("RTSP/1.0 %1 %2").arg(errcode).arg(errString);
	return resp;
}

QStringList RtspOutput::createDescribeResponse(int cseq, QString url, QString lsep)
{
	QStringList resp;
	int sdpSize = 0;
	QStringList sdp = createSdp(url);
	foreach (const QString &sdpline, sdp)
		sdpSize += sdpline.length() + lsep.length();
	resp << "RTSP/1.0 200 OK";
	resp << "Content-type: application/sdp";
	resp << "Cache-Control: no-cache";
	resp << QString("Content-Length: %1").arg(sdpSize);
	resp << QString("Content-Base: %1").arg(url);
	resp << QString("CSeq: %1").arg(cseq);
	resp << lsep;
	resp << sdp;
	resp << lsep;
	return resp;
}

/*
 * TODO: Change stream names to:
 *		stream1 -> full resolution, unicast
 *		stream1m -> full resolution, multicast
 *		stream2 -> quad resolution, 15 fps, unicast
 *		stream2m -> quad resolution, 15 fps, multicast
 *
 * Required header fields according to rfc2326:
 *
 * Connection, Content-Encoding, Content-Language, Content-Type,
 * CSeq, Proxy-Require, Require, RTP-Info, Session, Transport,
 * Unsupported
 */
QStringList RtspOutput::handleRtspMessage(QString mes, QString lsep, QString peerIp)
{
	QStringList resp;
	QStringList lines = mes.split(lsep);
	if (!lines.last().isEmpty()) {
		mDebug("un-espected last line");
		return resp;
	}
	if (lines.first().startsWith("OPTIONS")) {
		mDebug("handling options directive");
		int cseq = lines[1].remove("CSeq: ").toInt();
		resp << "RTSP/1.0 200 OK";
		resp << QString("CSeq: %1").arg(cseq);
		resp << "Public: DESCRIBE, SETUP, TEARDOWN, PLAY";
		resp << lsep;
	} else if (lines.first().startsWith("DESCRIBE")) {
		mDebug("handling describe directive");
		QString url = lines[0].split(" ")[1];
		int cseq = lines[1].remove("CSeq: ").toInt();
		resp << createDescribeResponse(cseq, url, lsep);
		/* TODO: check URL */
	} else if (lines.first().startsWith("SETUP")) {
		mDebug("handling setup directive");
		QString cbase = lines[0].split(" ")[1];

		QStringList fields = cbase.split("/", QString::SkipEmptyParts);
		if (fields.size() >= 3) {
			bool multicast = false;
			if (fields[2] == "stream1m")
				multicast = true;
			int cseq = lines[1].remove("CSeq: ").toInt();
			int dataPort = 0, controlPort = 0;
			foreach(QString line, lines) {
				if (line.startsWith("Require"))
					return createRtspErrorResponse(551);
				if (line.contains("Transport:")) {
					mDebug("setup transport line coming from client is: %s", qPrintable(line));
					QStringList fields2 = line.remove("Transport:").split(";");
					foreach(QString field, fields2) {
						if (field.contains("client_port") || field.contains("port=")) {
							/* TODO: check field sizes */
							QStringList ports = field.split("=");
							if (ports.size() >= 2) {
								dataPort = ports[1].split("-")[0].toInt();
								controlPort = ports[1].split("-")[1].toInt();
							}
						}
					}
				}
			}
			RtspSession *ses = findSession(multicast);
			if (!ses) {
				mDebug("cannot create more sessions");
				return createRtspErrorResponse(403);
			}
			int err = ses->setup(multicast, dataPort, controlPort, peerIp);
			if (err) {
				mDebug("cannot create session, error is %d", err);
				delete ses;
				return createRtspErrorResponse(err);
			}
			ses->controlUrl = cbase;
			sessions.insert(cbase, ses);

			resp << "RTSP/1.0 200 OK";
			resp << ses->transportString;
			resp << QString("Session: %1").arg(ses->sessionId);
			resp << "Content-Length: 0";
			resp << "Cache-Control: no-cache";
			resp << QString("CSeq: %1").arg(cseq);
			resp << lsep;
		} else {
			mDebug("not enough fields in setup url");
			return createRtspErrorResponse(400);
		}
	} else if (lines.first().startsWith("PLAY")) {
		mDebug("handling play directive");
		QString url = lines[0].split(" ")[1];
		if (sessions.contains(url)) {
			RtspSession *ses = sessions[url];
			int cseq = lines[1].remove("CSeq: ").toInt();
			resp << "RTSP/1.0 200 OK";
			resp << QString("RTP-Info: %1").arg(ses->rtpInfo());
			resp << QString("Session: %1").arg(ses->sessionId);
			resp << "Content-Length: 0";
			resp << "Cache-Control: no-cache";
			resp << QString("CSeq: %1").arg(cseq);
			resp << lsep;
			int err = ses->play();
			if (err)
				return createRtspErrorResponse(err);
		} else
			return createRtspErrorResponse(404);
	} else if (lines.first().startsWith("TEARDOWN")) {
		mDebug("handling teardown directive");
		int cseq = lines[1].remove("CSeq: ").toInt();
		QString url = lines[0].split(" ")[1];
		if (sessions.contains(url)) {
			RtspSession *ses = sessions[url];
			int err = ses->teardown(peerIp);
			if (err)
				return createRtspErrorResponse(err);
			resp << "RTSP/1.0 200 OK";
			resp << QString("Session: %1").arg(ses->sessionId);
			resp << "Content-Length: 0";
			resp << "Cache-Control: no-cache";
			resp << QString("CSeq: %1").arg(cseq);
			resp << lsep;
		} else
			return createRtspErrorResponse(400);
	} else {
		mDebug("Unknown RTSP directive:\n %s", qPrintable(mes));
		return createRtspErrorResponse(501);
	}

	return resp;
}

void RtspOutput::sendRtspMessage(QTcpSocket *sock, const QByteArray &mes)
{
	qDebug() << "sending rtsp message to" << sock->peerAddress() << QString::fromUtf8(mes);
	sock->write(mes);
}

void RtspOutput::sendRtspMessage(QTcpSocket *sock, const QStringList &lines, const QString &lsep)
{
	//qDebug() << "sending mes:\n" << lines;
	sendRtspMessage(sock, lines.join(lsep).toUtf8());
}

QStringList RtspOutput::createSdp(QString url)
{
	//Url format: rtsp://192.168.1.196:554/cam264.sdp
	QStringList fields = url.split("/", QString::SkipEmptyParts);
	if (fields.size() < 3)
		return QStringList();
	QStringList sdp;
	sdp << "m=video 0 RTP/AVP 96";
	sdp << "a=rtpmap:96 H264/90000";
	sdp << "a=fmtp:96 packetization-mode=1";
	sdp << "a=mimetype:string;\"video/h264\"";
	sdp << QString("a=control:%1").arg(url);
	if (fields[2] == "stream1m") {
		sdp << "c=IN IP4 224.1.1.1/4"; //TODO: Parametrize multicast address
	}
	return sdp;
}

QString RtspOutput::forwardToVlc(const QString &mes)
{
	QString mes2 = mes;
	mes2.replace("rtsp://192.168.1.196:554/cam264.sdp", "rtsp://localhost:8080/cam264.sdp");
	qDebug() << "forwarding to vlc:\n" << mes2;
	fwdSock->write(mes2.toUtf8());
	vlcWait->exec();
	qDebug() << "got response from vlc";
	return lastVlcData.replace("rtsp://localhost:8080/cam264.sdp", "rtsp://192.168.1.196:554/cam264.sdp");
}

bool RtspOutput::canSetupMore(bool multicast)
{
	if (multicast) {
		return true;
	} else {
		if (!sessions.size())
			return true;
		foreach (RtspSession *s, sessions) {
			if (s->state == RtspSession::TEARDOWN)
				return true;
		}
	}
	return false;
}

RtspSession * RtspOutput::findSession(bool multicast, QString url, QString sessionId)
{
	if (!sessions.size())
		return new RtspSession;
	foreach (RtspSession *s, sessions) {
		if (!url.isEmpty() && (s->controlUrl != url))
			continue;
		if (!sessionId.isEmpty() && (s->sessionId != sessionId))
			continue;
		if (multicast) {
			if (s->multicast) {
				return s;
			}
		} else {
			if (s->state == RtspSession::TEARDOWN)
				return s;
		}
	}
	return NULL;
}
