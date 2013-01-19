#include "basertspserver.h"

#include "rtph264mux.h"
#include "rtpmjpegmux.h"
#include "rawbuffer.h"

#include <emdesk/debug.h>

#include <errno.h>

#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalMapper>
#include <QStringList>
#include <QUuid>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <QDateTime>

/**
 *
 * Stream mapping:
 *		Application specific...
 *		.
 *		.
 *		.
 * Session management:
 *		- limited number of unicast sessions
 *		- multicast session start/stop
 *		- session keep alives and collection/restoration
 *		- endpoints/mux elements can be generated
 *		  dynamically or they can be statically
 *		  allocated
 *		-
 *
 * SDP creation:
 *		can use rtpmux
 *		can be hardcoded, simple string replacement
 *		can be generated by server
 *
 * Message parsing:
 *		Like html messages
 *
 * TCP socket management:
 *
 *
 * Buffer management:
 *		Buffers should be forwarded to mux elements
 *		Element statistics should be
 *
 * Security:
 *		-
 *
 * ====================================
 *
 * simple  <---------------------> complex
 * robust  <---------------------> feature rich
 *
 * So simplicity should be selectable. Use -> inheritance
 *
 * Base implementation:
 *
 *	- Message parsing
 *		- Support mandatory options only
 *	- No session management
 *		- Emit signals when new connection is ready
 *		- Do not manage mux elements
 *	- No security
 *	- No tcp socket management, close when request is handled
 *
 */

class BaseRtspSession
{
public:
	BaseRtspSession(BaseRtspServer *parent)
	{
		server = parent;
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
	~BaseRtspSession()
	{
	}

	int setup(bool mcast, int dPort, int cPort, QString streamName)
	{
		multicast = mcast;
		dataPort = dPort;
		controlPort = cPort;

		/* local port detection */
		QUdpSocket sock;
		sock.bind(QHostAddress(myIpAddr), 0);
		if (sock.localPort() & 0x1) {
			sourceDataPort = sock.localPort() - 1;
			sourceControlPort = sock.localPort();
		} else {
			sourceDataPort = sock.localPort();
			sourceControlPort = sock.localPort() + 1;
		}

		if (server->getSessionCodec(streamName) != Lmm::CODEC_H264
				|| server->getSessionCodec(streamName) == Lmm::CODEC_JPEG)
			return -EINVAL;

		if (multicast) {
			transportString = QString("Transport: RTP/AVP;multicast;destination=%3;port=%1-%2;ssrc=6335D514;mode=play")
					.arg(dataPort).arg(controlPort).arg(streamIp);
		} else {
			transportString = QString("Transport: RTP/AVP/UDP;unicast;client_port=%1-%2;server_port=%3-%4;ssrc=6335D514;mode=play")
					.arg(dataPort).arg(controlPort).arg(sourceDataPort)
					.arg(sourceControlPort);
		}
		/* create session identifier */
		sessionId = QUuid::createUuid().toString().split("-")[4].remove("}");
		state = SETUP;
		return 0;
	}
	QString rtpInfo()
	{
		if (multicast)
			return QString("url=rtsp://%1/%2;seq=6666;rtptime=1578998879").arg(myIpAddr.toString()).arg(streamName);
		return QString("url=rtsp://%1/%2;seq=6666;rtptime=1578998879").arg(myIpAddr.toString()).arg(streamName);
	}
	int play()
	{
		if (state != PLAY) {
			state = PLAY;
		}
		return 0;
	}
	int teardown()
	{
		if (state == PLAY) {
			state = TEARDOWN;
		}
		return 0;
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
	QString streamName;
	int sourceDataPort;
	int sourceControlPort;
	int dataPort;
	int controlPort;
	QString peerIp;
	QString streamIp;
private:
	QHostAddress myIpAddr;
	BaseRtspServer *server;
};

BaseRtspServer::BaseRtspServer(QObject *parent) :
	QObject(parent)
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
}

RtspSessionParameters BaseRtspServer::getSessionParameters(QString id)
{
	BaseRtspSession *ses = sessions[id];
	RtspSessionParameters sp;
	sp.controlPort = ses->controlPort;
	sp.controlUrl = ses->controlUrl;
	sp.dataPort = ses->dataPort;
	sp.multicast = ses->multicast;
	sp.sessionId = ses->sessionId;
	sp.sourceControlPort = ses->sourceControlPort;
	sp.sourceDataPort = ses->sourceDataPort;
	sp.transportString = ses->transportString;
	sp.peerIp = ses->peerIp;
	sp.streamIp = ses->streamIp;
	sp.streamName = ses->streamName;
	return sp;
}

QString BaseRtspServer::getMulticastAddress(QString)
{
	return "224.1.1.1";
}

QString BaseRtspServer::detectLineSeperator(QString mes)
{
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
	return lsep;
}

QString BaseRtspServer::getField(const QStringList lines, QString desc)
{
	foreach (QString line, lines) {
		if (line.contains(QString("%1:").arg(desc), Qt::CaseInsensitive)) {
			return line.split(":")[1].trimmed();
		}
	}
	return "";
}

void BaseRtspServer::newRtspConnection()
{
	QTcpSocket *sock = server->nextPendingConnection();
	connect(sock, SIGNAL(disconnected()), mapperDis, SLOT(map()));
	connect(sock, SIGNAL(error(QAbstractSocket::SocketError)), mapperErr, SLOT(map()));
	connect(sock, SIGNAL(readyRead()), mapperRead, SLOT(map()));
	mapperDis->setMapping(sock, sock);
	mapperErr->setMapping(sock, sock);
	mapperRead->setMapping(sock, sock);
	mDebug("client %s connected", qPrintable(sock->peerAddress().toString()));
}

void BaseRtspServer::clientDisconnected(QObject *obj)
{
	QTcpSocket *sock = (QTcpSocket *)obj;
	sock->abort();
	sock->deleteLater();
}

void BaseRtspServer::clientError(QObject *)
{
}

void BaseRtspServer::clientDataReady(QObject *obj)
{
	QTcpSocket *sock = (QTcpSocket *)obj;
	/* RTSP protocol uses UTF-8 */
	QString str = QString::fromUtf8(sock->readAll());
	QString mes = msgbuffer[sock].append(str);
	/* RTSP line sepeators should be decided by clients */
	QString lsep = detectLineSeperator(mes);
	/* end of message is 2 line seperators, by the standard */
	QString end = lsep + lsep;
	if (mes.contains(end)) {
		mDebug("new message from rtsp client: \n%s", qPrintable(mes));
		currentPeerIp = sock->peerAddress().toString();
		QStringList resp = handleRtspMessage(mes, lsep);
		msgbuffer[sock] = "";
		if (resp.size())
			sendRtspMessage(sock, resp, lsep);
	} else {
		/* message is splitted in multiple requests */
		msgbuffer[sock] = mes;
	}
}

QStringList BaseRtspServer::createRtspErrorResponse(int errcode, QString lsep)
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
	resp << lsep;
	return resp;
}

QStringList BaseRtspServer::createDescribeResponse(int cseq, QString url, QString lsep)
{
	QStringList resp;
	int sdpSize = 0;
	QStringList sdp = createSdp(url);
	foreach (const QString &sdpline, sdp)
		sdpSize += sdpline.length();
	resp << "RTSP/1.0 200 OK";
	resp << QString("CSeq: %1").arg(cseq);
	resp << QString("Date: %1 GMT").arg(QDateTime::currentDateTime().toString("ddd, MM MMM yyyy hh mm ss"));
	resp << QString("Content-Base: %1").arg(url);
	resp << "Content-Type: application/sdp";
	resp << QString("Content-Length: %1").arg(sdpSize + lsep.length() * sdp.length());
	resp << "";
	resp << sdp;
	resp << "";
	return resp;
}

QStringList BaseRtspServer::handleCommandOptions(QStringList lines, QString lsep)
{
	QStringList resp;
	mDebug("handling options directive");
	int cseq = lines[1].remove("CSeq: ").toInt();
	resp << "RTSP/1.0 200 OK";
	resp << QString("CSeq: %1").arg(cseq);
	resp << "Public: DESCRIBE, SETUP, TEARDOWN, PLAY";
	resp << lsep;
	return resp;
}

QStringList BaseRtspServer::handleCommandDescribe(QStringList lines, QString lsep)
{
	QStringList resp;
	mDebug("handling describe directive");
	QString url = lines[0].split(" ")[1];
	int cseq = lines[1].remove("CSeq: ").toInt();
	resp << createDescribeResponse(cseq, url, lsep);
	/* TODO: check URL */
	return resp;
}

QStringList BaseRtspServer::handleCommandSetup(QStringList lines, QString lsep)
{
	QStringList resp;
	mDebug("handling setup directive");
	QString cbase = lines[0].split(" ")[1];
	if (!cbase.endsWith("/"))
		cbase.append("/");

	QStringList fields = cbase.split("/", QString::SkipEmptyParts);
	QString stream = fields[2];
	if (fields.size() >= 3) {
		bool multicast = isMulticast(stream);
		int cseq = lines[1].remove("CSeq: ").toInt();
		int dataPort = 0, controlPort = 0;
		foreach(QString line, lines) {
			if (line.startsWith("Require"))
				return createRtspErrorResponse(551, lsep);
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
		BaseRtspSession *ses = new BaseRtspSession(this);
		ses->peerIp = currentPeerIp;
		if (multicast)
			ses->streamIp = getMulticastAddress(stream);
		else
			ses->streamIp = ses->peerIp;
		ses->controlUrl = cbase;
		ses->streamName = stream;
		int err = ses->setup(multicast, dataPort, controlPort, stream);
		if (err) {
			mDebug("cannot create session, error is %d", err);
			delete ses;
			return createRtspErrorResponse(err, lsep);
		}
		sessions.insert(ses->sessionId, ses);
		err = sessionSetupExtra(ses->sessionId);
		if (err) {
			mDebug("cannot setup session in sub-class, error is %d", err);
			sessions.remove(ses->sessionId);
			delete ses;
			return createRtspErrorResponse(err, lsep);
		}
		emit sessionSettedUp(ses->sessionId);

		resp << "RTSP/1.0 200 OK";
		resp << QString("CSeq: %1").arg(cseq);
		resp << ses->transportString;
		resp << QString("Session: %1").arg(ses->sessionId);
		resp << "Content-Length: 0";
		resp << "Cache-Control: no-cache";
		resp << lsep;
	} else {
		mDebug("not enough fields in setup url");
		return createRtspErrorResponse(400, lsep);
	}
	return resp;
}

QStringList BaseRtspServer::handleCommandPlay(QStringList lines, QString lsep)
{
	QStringList resp;
	QString url = lines[0].split(" ")[1];
	if (!url.endsWith("/"))
		url.append("/");
	mDebug("handling play directive: %s", qPrintable(url));
	QString sid = getField(lines, "Session");
	if (sessions.contains(sid)) {
		BaseRtspSession *ses = sessions[sid];
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
			return createRtspErrorResponse(err, lsep);
		sessionPlayExtra(ses->sessionId);
		emit sessionPlayed(ses->sessionId);
	} else
		return createRtspErrorResponse(404, lsep);
	return resp;
}

QStringList BaseRtspServer::handleCommandTeardown(QStringList lines, QString lsep)
{
	QStringList resp;
	mDebug("handling teardown directive");
	int cseq = lines[1].remove("CSeq: ").toInt();
	QString url = lines[0].split(" ")[1];
	if (!url.endsWith("/"))
		url.append("/");
	QString sid = getField(lines, "Session");
	if (sessions.contains(sid)) {
		BaseRtspSession *ses = sessions[sid];
		int err = ses->teardown();
		if (err)
			return createRtspErrorResponse(err, lsep);
		resp << "RTSP/1.0 200 OK";
		resp << QString("Session: %1").arg(ses->sessionId);
		resp << "Content-Length: 0";
		resp << "Cache-Control: no-cache";
		resp << QString("CSeq: %1").arg(cseq);
		resp << lsep;

		sessionTeardownExtra(ses->sessionId);
		emit sessionTearedDown(ses->sessionId);
		sessions.remove(sid);
		delete ses;
	} else
		return createRtspErrorResponse(400, lsep);
	return resp;
}

QStringList BaseRtspServer::handleRtspMessage(QString mes, QString lsep)
{
	QStringList resp;
	QStringList lines = mes.split(lsep);
	if (!lines.last().isEmpty()) {
		mDebug("un-espected last line");
		return resp;
	}
	if (lines.first().startsWith("OPTIONS")) {
		resp = handleCommandOptions(lines, lsep);
	} else if (lines.first().startsWith("DESCRIBE")) {
		resp = handleCommandDescribe(lines, lsep);
	} else if (lines.first().startsWith("SETUP")) {
		resp = handleCommandSetup(lines, lsep);
	} else if (lines.first().startsWith("PLAY")) {
		resp = handleCommandPlay(lines, lsep);
	} else if (lines.first().startsWith("TEARDOWN")) {
		resp = handleCommandTeardown(lines, lsep);
	} else {
		mDebug("Unknown RTSP directive:\n %s", qPrintable(mes));
		return createRtspErrorResponse(501, lsep);
	}

	return resp;
}

void BaseRtspServer::sendRtspMessage(QTcpSocket *sock, const QByteArray &mes)
{
	mDebug("sending rtsp message to %s: %s", qPrintable(sock->peerAddress().toString()),
		   mes.constData());
	sock->write(mes);
}

void BaseRtspServer::sendRtspMessage(QTcpSocket *sock, QStringList &lines, const QString &lsep)
{
	sendRtspMessage(sock, lines.join(lsep).toUtf8());
}

QStringList BaseRtspServer::createSdp(QString url)
{
	QStringList fields = url.split("/", QString::SkipEmptyParts);
	QString stream = fields[2];
	QStringList sdp;
	Lmm::CodecType codec = getSessionCodec(stream);
	QString dstIp = currentPeerIp;
	if (isMulticast(stream))
		dstIp = getMulticastAddress(stream);
	if (codec == Lmm::CODEC_H264) {
		sdp << "v=0";
		sdp << "o=- 0 0 IN IP4 127.0.0.1";
		sdp << "s=No Name";
		sdp << "c=IN IP4 " + dstIp;
		sdp << "t=0 0";
		sdp << "a=tool:libavformat 52.102.0";
		sdp << "m=video 15678 RTP/AVP 96";
		sdp << "a=rtpmap:96 H264/90000";
		sdp << "a=fmtp:96 packetization-mode=1";
		sdp << QString("a=control:%1").arg(url);
	} else if (codec == Lmm::CODEC_JPEG) {
		sdp << "v=0";
		sdp << "o=- 0 0 IN IP4 127.0.0.1";
		sdp << "s=No Name";
		sdp << "c=IN IP4 " + dstIp;
		sdp << "t=0 0";
		sdp << "a=tool:libavformat 52.102.0";
		sdp << "m=video 15678 RTP/AVP 26";
	}

	return sdp;
}
