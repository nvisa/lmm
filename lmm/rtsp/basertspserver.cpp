#include "basertspserver.h"
#include "rawbuffer.h"
#include "rtp/rtptransmitter.h"
#include "tools/tokenbucket.h"
#include "platform_info.h"

#include "debug.h"

#include <errno.h>

#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDataStream>
#include <QSignalMapper>
#include <QStringList>
#include <QUuid>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <QDateTime>
#include <QMutex>
#include <QElapsedTimer>

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

class MyTime
{
public:
	MyTime()
	{

	}
	int elapsed()
	{
		l.lock();
		int el = t.elapsed();
		l.unlock();
		return el;
	}
	int restart()
	{
		l.lock();
		int el = t.restart();
		l.unlock();
		return el;
	}
	void start()
	{
		l.lock();
		t.start();
		l.unlock();
	}
protected:
	QElapsedTimer t;
	QMutex l;
};

static QHostAddress findIp(const QString &ifname)
{
	QHostAddress myIpAddr;
	/* Let's find our IP address */
	foreach (const QNetworkInterface iface, QNetworkInterface::allInterfaces()) {
		if (iface.name() != ifname)
			continue;
		if (iface.addressEntries().size()) {
			myIpAddr = iface.addressEntries().at(0).ip();
			break;
		}
	}
	return myIpAddr;
}

static inline QString createDateHeader()
{
	return QString("Date: %1 GMT").arg(QDateTime::currentDateTime().toUTC().toString("ddd, dd MMM yyyy hh:mm:ss"));
}

BaseRtspServer::BaseRtspServer(QObject *parent, int port) :
	QObject(parent)
{
	serverPort = port;
	nwInterfaceName = "eth0";
	auth = AUTH_NONE;
	enabled = true;
	server = new QTcpServer(this);
	connect(server, SIGNAL(newConnection()), SLOT(newRtspConnection()));
	mapperDis = new QSignalMapper(this);
	mapperErr = new QSignalMapper(this);
	mapperRead = new QSignalMapper(this);
	connect(mapperDis, SIGNAL(mapped(QObject*)), SLOT(clientDisconnected(QObject*)));
	connect(mapperErr, SIGNAL(mapped(QObject*)), SLOT(clientError(QObject*)));
	connect(mapperRead, SIGNAL(mapped(QObject*)), SLOT(clientDataReady(QObject*)));

	myIpAddr = findIp(nwInterfaceName);
	connect(this, SIGNAL(newRtpTcpData(QByteArray,RtpChannel*)), SLOT(handleNewRtpTcpData(QByteArray,RtpChannel*)), Qt::QueuedConnection);

	lastTunnellingSocket = NULL;

	setEnabled(enabled);
}

/**
 * @brief BaseRtspServer::addStream
 * @param streamName
 * @param multicast
 * @param port
 * @param mcastAddress
 *
 * This function adds a new stream to RTSP server. New stream is empty and doesn't contain any media implementations. So
 * this new stream will basically be useless until you add new media using addMedia2Stream() function.
 *
 * All media belonging to this stream will use the provided multicast information.
 */
void BaseRtspServer::addStream(const QString streamName, bool multicast, int port, const QString &mcastAddress)
{
	StreamDescription desc;
	desc.rtp = NULL;
	desc.multicast = multicast;
	desc.port = port;
	desc.multicastAddr = mcastAddress;
	desc.streamUrlSuffix = streamName;
	streamDescriptions.insert(streamName, desc);
}

/**
 * @brief BaseRtspServer::addStream
 * @param streamName
 * @param multicast
 * @param rtp
 * @param port
 * @param mcastAddress
 *
 * This is an overloaded function which adds a given RtpTransmitter as a new media to the newly created stream. Media name and
 * the stream name will be the same.
 */
void BaseRtspServer::addStream(const QString streamName, bool multicast, RtpTransmitter *rtp, int port, const QString &mcastAddress)
{
	StreamDescription desc;
	desc.streamUrlSuffix = streamName;
	desc.multicast = multicast;
	desc.rtp = rtp;
	desc.port = port;
	desc.multicastAddr = mcastAddress;
	desc.multicastAddressBase = "239.0.0.0";
	streamDescriptions.insert(streamName, desc);
}

void BaseRtspServer::addMedia2Stream(const QString &mediaName, const QString &streamName, bool multicast, RtpTransmitter *rtp, int port, const QString &mcastAddress)
{
	StreamDescription desc;
	desc.streamUrlSuffix = mediaName;
	desc.multicast = multicast;
	desc.rtp = rtp;
	desc.port = port;
	desc.multicastAddr = mcastAddress;
	desc.multicastAddressBase = "239.0.0.0";
	streamDescriptions[streamName].media.insert(mediaName, desc);
}

bool BaseRtspServer::hasStream(const QString &streamName)
{
	return streamDescriptions.contains(streamName);
}

void BaseRtspServer::addStreamParameter(const QString &streamName, const QString &mediaName, const QString &par, const QVariant &value)
{
	if (!streamDescriptions.contains(streamName))
		return;
	if (!streamDescriptions[streamName].media.contains(mediaName))
		return;
	streamDescriptions[streamName].media[mediaName].meta.insert(par, value);
}

const QHash<QString, QVariant> BaseRtspServer::getStreamParameters(const QString &streamName, const QString &mediaName)
{
	if (!streamDescriptions.contains(streamName))
		return QHash<QString, QVariant>();
	if (!streamDescriptions[streamName].media.contains(mediaName))
		return QHash<QString, QVariant>();
	return streamDescriptions[streamName].media[mediaName].meta;
}

void BaseRtspServer::setRtspAuthentication(BaseRtspServer::Auth authMethod)
{
	auth = authMethod;
}

void BaseRtspServer::setRtspAuthenticationCredentials(const QString &username, const QString &password)
{
	authUsername = username;
	authPassword = password;
}

const QStringList BaseRtspServer::getSessions()
{
	return sessions.keys();
}

const QStringList BaseRtspServer::getSessions(const QString &streamName)
{
	QStringList list;
	QMapIterator<QString, BaseRtspSession *>i(sessions);
	while (i.hasNext()) {
		i.next();
		BaseRtspSession *ses = i.value();
		if (ses->streamName == streamName)
			list << ses->sessionId;
	}
	return list;
}

const BaseRtspSession *BaseRtspServer::getSession(const QString &sid)
{
	QMutexLocker l(&sessionLock);
	if (sessions.contains(sid))
		return sessions[sid];
	return NULL;
}

void BaseRtspServer::saveSessions(const QString &filename)
{
	QMutexLocker l(&sessionLock);
	QByteArray ba;
	QDataStream out(&ba, QIODevice::WriteOnly);
	out.setByteOrder(QDataStream::LittleEndian);
	out << (quint32)0x78414117; //key
	out << (quint32)2;			//version
	QMapIterator<QString, BaseRtspSession *>i(sessions);
	while (i.hasNext()) {
		i.next();

		BaseRtspSession *ses = i.value();
		if (ses->state == BaseRtspSession::TEARDOWN)
			continue;

		out << i.key();

		out << (int)ses->state;
		out << ses->transportString;
		out << ses->sessionId;
		out << ses->multicast;
		out << ses->controlUrl;
		out << ses->streamName;
		out << ses->mediaName;
		out << ses->sourceDataPort;
		out << ses->sourceControlPort;
		out << ses->dataPort;
		out << ses->controlPort;
		out << ses->peerIp;
		out << ses->streamIp;
		out << ses->clientCount;
		out << ses->ssrc;
		out << ses->ttl;
		out << ses->timeout->elapsed();
		out << ses->rtptime;
		out << ses->seq;
		out << ses->rtspTimeoutEnabled;

		RtpChannel *ch = ses->getRtpChannel();
		out << ch->baseTs;
		out << ch->seq;
		out << ch->lastRtpTs;
	}


	QFile f(filename);
	if (f.open(QIODevice::WriteOnly))
		f.write(ba);
}

int BaseRtspServer::loadSessions(const QString &filename)
{
	QFile f(filename);
	if (!f.open(QIODevice::ReadOnly))
		return -ENOENT;
	QByteArray ba = f.readAll();
	f.close();

	QDataStream in(ba);
	in.setByteOrder(QDataStream::LittleEndian);
	quint32 key; in >> key;
	quint32 version; in >> version;
	mDebug("found session file with key 0x%x and version %d", key, version);
	if (key != 0x78414117 || version != 2)
		return -EINVAL;

	int count = 0;
	while (!in.atEnd()) {
		QString sessionId; in >> sessionId;
		BaseRtspSession *ses = new BaseRtspSession(nwInterfaceName, this);
		int state; in >> state;
		ses->state = (BaseRtspSession::SessionState)state;
		in >> ses->transportString;
		in >> ses->sessionId;
		in >> ses->multicast;
		in >> ses->controlUrl;
		in >> ses->streamName;
		in >> ses->mediaName;
		in >> ses->sourceDataPort;
		in >> ses->sourceControlPort;
		in >> ses->dataPort;
		in >> ses->controlPort;
		in >> ses->peerIp;
		in >> ses->streamIp;
		in >> ses->clientCount;
		in >> ses->ssrc;
		in >> ses->ttl;
		int elapsed; in >> elapsed;
		ses->timeout->start();
		in >> ses->rtptime;
		in >> ses->seq;
		in >> ses->rtspTimeoutEnabled;

		/* every session at least should be setted-up */
		ses->setup(ses->multicast, ses->dataPort, ses->controlPort, ses->streamName, ses->mediaName, "");

		in >> ses->getRtpChannel()->baseTs;
		in >> ses->getRtpChannel()->seq;
		in >> ses->getRtpChannel()->lastRtpTs;
		ses->getRtpChannel()->seq += 120;
		ses->getRtpChannel()->baseTs = ses->getRtpChannel()->lastRtpTs + 4 * 90000;

		if (state == BaseRtspSession::PLAY)
			ses->play();

		sessions.insert(sessionId, ses);
		count++;
	}

	QMapIterator<QString, BaseRtspSession *> mio(sessions);
	while (mio.hasNext()) {
		mio.next();
		BaseRtspSession *ses = mio.value();

		QMapIterator<QString, BaseRtspSession *> mi(sessions);
		while (mi.hasNext()) {
			mi.next();
			BaseRtspSession *sibling = mi.value();
			if (sibling->streamName != ses->streamName)
				continue;
			if (sibling->mediaName == ses->mediaName)
				continue;
			ses->siblings << sibling;
		}
	}

	return count;
}

int BaseRtspServer::newRtpData(const char *data, int size, RtpChannel *ch)
{
	emit newRtpTcpData(QByteArray(data, size), ch);
	return 0;
}

bool BaseRtspServer::detectLocalPorts(int &rtp, int &rtcp)
{
	return detectLocalPorts(myIpAddr, rtp, rtcp);
}

bool BaseRtspServer::detectLocalPorts(const QHostAddress &myIpAddr, int &rtp, int &rtcp)
{
	QUdpSocket sock, sock2;
	bool ok1 = sock.bind(myIpAddr, 0);
	if (!ok1)
		return false;
	bool ok2 = false;
	if (sock.localPort() & 0x1) {
		ok2 = sock2.bind(sock.localPort() - 1);
		rtp = sock2.localPort();
		rtcp = sock.localPort();
	} else {
		ok2 = sock2.bind(sock.localPort() + 1);
		rtp = sock.localPort();
		rtcp = sock2.localPort();
	}
	return ok2;
}

void BaseRtspServer::handleNewRtpTcpData(const QByteArray &ba, RtpChannel *ch)
{
	if (avpTcpMappings.contains(ch)) {
		QTcpSocket *sock = avpTcpMappings[ch];
		if (tunnellingMappings.contains(sock))
			sock = tunnellingMappings[sock];
		QByteArray data;
		data.append('$');
		data.append((char)0);
		data.append((char)(ba.size() >> 8));
		data.append((char)(ba.size() & 0xff));
		data.append(ba);
		sock->write(data);
	}
}

const BaseRtspServer::StreamDescription BaseRtspServer::getStreamDesc(const QString &streamName, const QString &mediaName)
{
	if (!streamDescriptions.contains(streamName))
		return StreamDescription();
	if (!streamDescriptions[streamName].media.contains(mediaName))
		return streamDescriptions[streamName];
	return streamDescriptions[streamName].media[mediaName];
}

/**
 * @brief BaseRtspServer::isMulticast
 * @param streamName Name of the desired stream.
 * @param media Specific media name in the stream, can be empty.
 * @return
 *
 * In case media is empty, this function returns stream-level defined multicast information.
 */
bool BaseRtspServer::isMulticast(QString streamName, const QString &media)
{
	const StreamDescription &desc = getStreamDesc(streamName, media);
	if (desc.streamUrlSuffix.isEmpty())
		return false;
	return desc.multicast;
}

RtpTransmitter * BaseRtspServer::getSessionTransmitter(const QString &streamName, const QString &media)
{
	const StreamDescription &desc = getStreamDesc(streamName, media);
	if (desc.streamUrlSuffix.isEmpty())
		return NULL;
	return desc.rtp;
}

/**
 * @brief BaseRtspServer::getMulticastAddress
 * @param streamName
 * @param media
 * @return
 *
 * This function behaves much like isMulticast() function.
 *
 * \sa isMulticast(), getMulticastAddress(), getMulticastPort()
 */
QString BaseRtspServer::getMulticastAddress(const QString &streamName, const QString &media)
{
	const StreamDescription &desc = getStreamDesc(streamName, media);
	if (desc.streamUrlSuffix.isEmpty())
		return "";
	if (!desc.multicastAddr.isEmpty())
		return desc.multicastAddr;

	QHostAddress ipAddr = myIpAddr;
	QHostAddress netmask = QHostAddress("255.0.0.0");
	quint32 addr = (netmask.toIPv4Address() & QHostAddress(desc.multicastAddressBase).toIPv4Address())
			| (~netmask.toIPv4Address() & ipAddr.toIPv4Address());
	return QHostAddress(addr).toString();
}

/**
 * @brief BaseRtspServer::getMulticastPort
 * @param streamName
 * @param media
 * @return
 *
 * This function behaves much like isMulticast() function.
 *
 * \sa isMulticast(), getMulticastAddress(), getMulticastAddress()
 */
int BaseRtspServer::getMulticastPort(QString streamName, const QString &media)
{
	const StreamDescription &desc = getStreamDesc(streamName, media);
	if (desc.streamUrlSuffix.isEmpty())
		return 0;
	return desc.port;
}

int BaseRtspServer::setEnabled(bool val)
{
	enabled = val;
	if (enabled) {
#if QT_VERSION < 0x050000
		QHostAddress::SpecialAddress bindAddr = QHostAddress::Any;
#else
		QHostAddress::SpecialAddress bindAddr = QHostAddress::AnyIPv4;
#endif
		if (!server->listen(bindAddr, serverPort))
			mDebug("unable to bind to tcp port %d", serverPort);
		return 0;
	} else {
		server->close();
	}
	QMapIterator<QString, BaseRtspSession *>i(sessions);
	while (i.hasNext()) {
		i.next();
		BaseRtspSession *ses = i.value();
		closeSession(ses->sessionId);
		/*QStringList resp;
		resp << "RTSP/1.0 200 OK";
		resp << QString("Session: %1").arg(ses->sessionId);
		resp << "Content-Length: 0";
		resp << "Cache-Control: no-cache";
		resp << QString("CSeq: %1").arg(cseq);
		resp << "\n\r";
		sendRtspMessage(sock, resp, lsep);*/
	}
	return 0;
}

QString BaseRtspServer::getMulticastAddressBase(const QString &streamName, const QString &media)
{
	return getStreamDesc(streamName, media).multicastAddressBase;
}

void BaseRtspServer::setMulticastAddressBase(const QString &streamName, const QString &media, const QString &addr)
{
	if (media.isEmpty())
		streamDescriptions[streamName].multicastAddressBase = addr;
	else
		streamDescriptions[streamName].media[media].multicastAddressBase = addr;
}

QString BaseRtspServer::detectLineSeperator(QString mes)
{
	QString lsep;
	if (mes.contains("\r\n")) {
		lsep = "\r\n";
		mInfo("detected lsep as \\r\\n");
	} else if (mes.contains("\n\r")) {
		lsep = "\n\r";
		mInfo("detected lsep as \\n\\r");
	} else if (mes.contains("\n")) {
		lsep = "\n";
		mInfo("detected lsep as \\n");
	} else if (mes.contains("\r")) {
		lsep = "\r";
		mInfo("detected lsep as \\r");
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

BaseRtspSession * BaseRtspServer::findMulticastSession(const QString &streamName, const QString &media)
{
	QMapIterator<QString, BaseRtspSession *> it(sessions);
	while (it.hasNext()) {
		it.next();
		BaseRtspSession *s = it.value();
		if (s->multicast && s->streamName == streamName && s->mediaName == media)
			return s;
	}
	return NULL;
}

bool BaseRtspServer::isSessionMulticast(QString sid)
{
	return sessions[sid]->multicast;
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
	if (avpTcpMappings.values().contains(sock)) {
		avpTcpMappings.remove(avpTcpMappings.key(sock));
	}
	if (tunnellingMappings.values().contains(sock))
		tunnellingMappings.remove(tunnellingMappings.key(sock));
	if (tunnellingMappings.contains(sock))
		tunnellingMappings.remove(sock);
}

void BaseRtspServer::clientError(QObject *)
{
}

void BaseRtspServer::clientDataReady(QObject *obj)
{
	QTcpSocket *sock = (QTcpSocket *)obj;
	currentSocket = sock;
	/* RTSP protocol uses UTF-8 */
	QString str = QString::fromUtf8(sock->readAll());
	QString mes = msgbuffer[sock].append(str);
	/* RTSP line sepeators should be decided by clients */
	QString lsep = detectLineSeperator(mes);
	if (mes.startsWith("POST") || tunnellingMappings.contains(sock) || lsep.isEmpty()) {
		handlePostData(sock, mes, lsep);
		msgbuffer[sock] = "";
		return;
	}
	/* end of message is 2 line seperators, by the standard */
	QString end = lsep + lsep;
	if (mes.contains(end)) {
		mInfo("new message from rtsp client: \n%s", qPrintable(mes));
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
	else if (errcode == 401)
		errString = "Unauthorized Status";
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
	else if (errcode == 503)
		errString = "Service Unavailable";
	resp << QString("RTSP/1.0 %1 %2").arg(errcode).arg(errString);
	if (errcode == 401)
		resp << "WWW-Authenticate: Basic realm=\"streaming\"";
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
	resp << createDateHeader();
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
	foreach (QString line, lines) {
		if (line.contains("user-agent", Qt::CaseInsensitive))
			lastUserAgent = line.split(":")[1];
	}

	mDebug("handling options directive");
	int cseq = currentCmdFields["CSeq"].toInt();
	resp << "RTSP/1.0 200 OK";
	resp << QString("CSeq: %1").arg(cseq);
	resp << createDateHeader();
	resp << "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, GET_PARAMETER, SET_PARAMETER";
	resp << lsep;
	return resp;
}

QStringList BaseRtspServer::handleCommandDescribe(QStringList lines, QString lsep)
{
	Q_UNUSED(lines);
	QStringList resp;
	mDebug("handling describe directive");
	QString url = currentCmdFields["url"];
	int cseq = currentCmdFields["CSeq"].toInt();
	resp << createDescribeResponse(cseq, url, lsep);
	/* TODO: check URL */
	return resp;
}

static int rtpTransportHook(const char *data, int size, RtpChannel *ch, void *priv)
{
	BaseRtspServer *server = (BaseRtspServer *)priv;
	return server->newRtpData(data, size, ch);
}

QStringList BaseRtspServer::handleCommandSetup(QStringList lines, QString lsep)
{
	QStringList resp;
	mDebug("handling setup directive");
	QString cbase = lines[0].split(" ")[1];
	if (!cbase.endsWith("/"))
		cbase.append("/");

	if (!enabled)
		return createRtspErrorResponse(503, lsep);
	QStringList fields = cbase.split("/", QString::SkipEmptyParts);
	if (fields.size() < 3)
		return createRtspErrorResponse(451, lsep);
	QString stream = fields[2];
	QString media;
	QString transportString;
	if (fields.size() > 3)
		media = fields[3];
	if (fields.size() >= 3) {
		bool multicast = isMulticast(stream, media);
		int cseq = currentCmdFields["CSeq"].toInt();
		int dataPort = 0, controlPort = 0;
		foreach(QString line, lines) {
			if (line.startsWith("Require"))
				return createRtspErrorResponse(551, lsep);
			if (line.contains("Transport:")) {
				mDebug("setup transport line coming from client is: %s", qPrintable(line));
				transportString = line.remove("Transport:").trimmed();
				QStringList fields2 = transportString.split(";");
				foreach(QString field, fields2) {
					if (field.contains("client_port") || field.contains("port=")) {
						/* TODO: check field sizes */
						QStringList ports = field.split("=");
						if (ports.size() >= 2) {
							dataPort = ports[1].split("-")[0].toInt();
							controlPort = ports[1].split("-")[1].toInt();
						}
					} else if (field.contains("unicast") && multicast)
						return createRtspErrorResponse(451, lsep);
					else if (field.contains("multicast") && !multicast)
						return createRtspErrorResponse(451, lsep);
				}
				if (!dataPort)
					dataPort = 5000;
				if (!controlPort)
					controlPort = dataPort + 1;
			}
		}
		BaseRtspSession *ses = findMulticastSession(stream, media);
		if (!ses) {
			ses = new BaseRtspSession(nwInterfaceName, this);
			ses->peerIp = currentSocket->peerAddress().toString();
			if (multicast)
				ses->streamIp = getMulticastAddress(stream, media);
			else
				ses->streamIp = ses->peerIp;
			ses->controlUrl = cbase;
			ses->streamName = stream;
			ses->mediaName = media;
			ses->ssrc = rand();
			ses->ttl = 10;
			int err = ses->setup(multicast, dataPort, controlPort, stream, media, transportString);
			if (err) {
				mDebug("cannot create session, error is %d", err);
				delete ses;
				return createRtspErrorResponse(err, lsep);
			}
			if (ses->rtpAvpTcp) {
				ses->getRtpChannel()->trHook = rtpTransportHook;
				ses->getRtpChannel()->trHookPriv = this;
				avpTcpMappings.insert(ses->getRtpChannel(), currentSocket);
			}
			sessions.insert(ses->sessionId, ses);
		} else {
			ses->clientCount++;
		}

		resp << "RTSP/1.0 200 OK";
		resp << QString("CSeq: %1").arg(cseq);
		resp << createDateHeader();
		resp << ses->transportString;
		resp << QString("Session:%1;timeout=%2").arg(ses->sessionId).arg(60);
		resp << "Content-Length: 0";
		resp << "Cache-Control: no-cache";
		resp << lsep;

		/*
		 * we are not defining timeout value, so default
		 * value of 60 seconds is used.
		 */
		ses->timeout->start();
	} else {
		mDebug("not enough fields in setup url");
		return createRtspErrorResponse(400, lsep);
	}
	return resp;
}

QStringList BaseRtspServer::handleCommandPlay(QStringList lines, QString lsep)
{
	QStringList resp;
	QString url = currentCmdFields["url"];
	if (!url.endsWith("/"))
		url.append("/");
	QStringList fields = url.split("/", QString::SkipEmptyParts);
	if (fields.size() < 3)
		return createRtspErrorResponse(451, lsep);
	QString stream = fields[2];
	QString media;
	if (fields.size() > 3)
		media = fields[3];
	mDebug("handling play directive: %s", qPrintable(url));
	QString sid = getField(lines, "Session");
	if (sessions.contains(sid)) {
		BaseRtspSession *ses = sessions[sid];
		int cseq = currentCmdFields["CSeq"].toInt();
		if (ses->clientCount == 1) {
			int err = ses->play();
			if (err)
				return createRtspErrorResponse(err, lsep);
			ses->seq = getSessionBaseSequence(ses->sessionId);
			ses->rtptime = getSessionBaseTimestamp(ses->sessionId);
		}
		/* kick time-out value */
		ses->timeout->restart();
		/* create response */
		resp << "RTSP/1.0 200 OK";
		resp << QString("CSeq: %1").arg(cseq);
		resp << createDateHeader();
		resp << QString("Range: npt=0.000-");
		resp << QString("RTP-Info: %1").arg(ses->rtpInfo());
		resp << QString("Session: %1").arg(ses->sessionId);
		resp << "Content-Length: 0";
		resp << "Cache-Control: no-cache";
		resp << lsep;

		if (media.isEmpty()) {
			/* check for aggregate PLAY operations */
			QMapIterator<QString, BaseRtspSession *> mi(sessions);
			while (mi.hasNext()) {
				mi.next();
				BaseRtspSession *sibling = mi.value();
				if (sibling->state == BaseRtspSession::PLAY)
					continue;
				if (sibling->streamName != stream)
					continue;
				if (sibling->mediaName == media)
					continue;
				/* not playing *different* media session within our stream, so let's start it */
				if (sibling->clientCount == 1) {
					int err = sibling->play();
					if (err)
						return createRtspErrorResponse(err, lsep);
					sibling->seq = getSessionBaseSequence(sibling->sessionId);
					sibling->rtptime = getSessionBaseTimestamp(sibling->sessionId);
				}
				/* kick time-out value */
				sibling->timeout->restart();
				ses->siblings << sibling;
			}
		}
	} else
		return createRtspErrorResponse(404, lsep);
	return resp;
}

QStringList BaseRtspServer::handleCommandTeardown(QStringList lines, QString lsep)
{
	QStringList resp;
	mDebug("handling teardown directive");
	int cseq = currentCmdFields["CSeq"].toInt();
	QString url = currentCmdFields["url"];
	if (!url.endsWith("/"))
		url.append("/");
	QStringList fields = url.split("/", QString::SkipEmptyParts);
	if (fields.size() < 3)
		return createRtspErrorResponse(451, lsep);
	QString stream = fields[2];
	QString media;
	if (fields.size() > 3)
		media = fields[3];
	QString sid = getField(lines, "Session");
	if (sessions.contains(sid)) {
		if (media.isEmpty()) {
			/* check for aggregate TEARDOWN operations */
			foreach (BaseRtspSession *sibling, sessions[sid]->siblings)
				closeSession(sibling->sessionId);
		}

		closeSession(sid);
		resp << "RTSP/1.0 200 OK";
		resp << QString("CSeq: %1").arg(cseq);
		resp << createDateHeader();
		resp << QString("Session: %1").arg(sid);
		resp << "Content-Length: 0";
		resp << "Cache-Control: no-cache";
		resp << lsep;
	} else
		return createRtspErrorResponse(400, lsep);
	return resp;
}

QStringList BaseRtspServer::handleCommandGetParameter(QStringList lines, QString lsep)
{
	QStringList resp;
	mInfo("handling get_parameter directive");
	int cseq = currentCmdFields["CSeq"].toInt();
	QString url = currentCmdFields["url"];
	if (!url.endsWith("/"))
		url.append("/");
	QString sid = getField(lines, "Session");
	if (sessions.contains(sid)) {
		BaseRtspSession *ses = sessions[sid];
		ses->timeout->restart();
		ses->rtspTimeoutEnabled = true;
		/* we need to kick-out related sessions as well */
		foreach (BaseRtspSession *sibling, ses->siblings) {
			sibling->timeout->restart();
			sibling->rtspTimeoutEnabled = true;
		}

		resp << "RTSP/1.0 200 OK";
		resp << QString("CSeq: %1").arg(cseq);
		resp << createDateHeader();
		resp << QString("Session: %1").arg(ses->sessionId);
		resp << "Content-Length: 0";
		resp << lsep;
	} else
		return createRtspErrorResponse(400, lsep);
	return resp;
}

QStringList BaseRtspServer::handleCommandSetParameter(QStringList lines, QString lsep)
{
	QStringList resp;
	mInfo("handling set_parameter directive");
	int cseq = currentCmdFields["CSeq"].toInt();
	QString url = currentCmdFields["url"];
	if (!url.endsWith("/"))
		url.append("/");
	QString sid = getField(lines, "Session");
	if (sessions.contains(sid)) {
		BaseRtspSession *ses = sessions[sid];
		ses->timeout->restart();
		ses->rtspTimeoutEnabled = true;
		/* we need to kick-out related sessions as well */
		foreach (BaseRtspSession *sibling, ses->siblings) {
			sibling->timeout->restart();
			sibling->rtspTimeoutEnabled = true;
		}

		resp << "RTSP/1.0 200 OK";
		resp << QString("CSeq: %1").arg(cseq);
		resp << createDateHeader();
		resp << QString("Session: %1").arg(ses->sessionId);
		resp << "Content-Length: 0";
		resp << lsep;
	} else
		return createRtspErrorResponse(400, lsep);
	return resp;
}

uint BaseRtspServer::getSessionBaseTimestamp(QString sid)
{
	if (!sessions.contains(sid))
		return 0;
	return sessions[sid]->getRtpChannel()->baseTs;
}

uint BaseRtspServer::getSessionBaseSequence(QString sid)
{
	if (!sessions.contains(sid))
		return 0;
	return sessions[sid]->getRtpChannel()->seq;
}

void BaseRtspServer::closeSession(QString sessionId)
{
	mDebug("closing session %s", qPrintable(sessionId));
	if (!sessions.contains(sessionId))
		return;
	BaseRtspSession *ses = sessions[sessionId];
	if (ses->clientCount == 1) {
		ses->teardown();
		sessions.remove(sessionId);
		ses->deleteLater();
	} else
		ses->clientCount--;
}

QString BaseRtspServer::getEndpointAddress()
{
	if (server->serverPort() == 554)
		return myIpAddr.toString();
	return QString("%1:%2").arg(server->serverPort());
}

void BaseRtspServer::handlePostData(QTcpSocket *sock, QString mes, QString lsep)
{
	/*
	 * We have 4 cases:
	 *	1. Message starts with POST message and ends with 2-lsep
	 *	2. Message starts with POST and doesn't end with 2-lsep
	 *	3. Message doesn't start with POST
	 *	4. Message is a plain RTSP directive
	 *
	 */
	QString l2sep = lsep + lsep;
	if (mes.startsWith("POST")) {
		if (lastTunnellingSocket) {
			tunnellingMappings.insert(sock, lastTunnellingSocket);
			lastTunnellingSocket = NULL;
		}
		if (mes.endsWith(l2sep))
			return;
		if (!mes.contains(l2sep))
			return;
		QStringList list = mes.split(l2sep, QString::SkipEmptyParts);
		foreach (QString lmes, list) {
			if (lmes.startsWith("POST"))
				continue;
			handlePostData(sock, lmes, lsep);
		}
		return;
	}

	if (!lsep.isEmpty() && mes.contains(lsep)) {
		/*
		 * ONVIF has a weird test-case where they send base64 message
		 * with line-breaks. They are really pushing it hard to make
		 * a conformant product!
		 */
		if (mes.contains("rtsp://")) {
			/* case 4 */
			mInfo("case 4: \n%s", qPrintable(mes));
			if (!mes.endsWith(l2sep))
				mes.append(l2sep);
			QStringList lines = handleRtspMessage(mes, lsep);
			if (tunnellingMappings.contains(sock))
				sendRtspMessage(tunnellingMappings[sock], lines, lsep);
			else
				sendRtspMessage(sock, lines, lsep);
			return;
		}
		mes = mes.remove(lsep);
	}

	/* case 3 */
	mes = QString::fromUtf8(QByteArray::fromBase64(mes.toUtf8()));
	lsep = detectLineSeperator(mes);
	l2sep = lsep + lsep;
	QStringList list = mes.split(l2sep, QString::SkipEmptyParts);
	foreach (QString lmes, list)
		handlePostData(sock, lmes, lsep);
}

QStringList BaseRtspServer::handleRtspMessage(QString mes, QString lsep)
{
	QStringList resp;
	QStringList lines = mes.split(lsep);
	if (!lines.last().isEmpty() || lines.first().isEmpty()) {
		mDebug("un-espected last line: \n%s", qPrintable(mes));
		return resp;
	}
	currentCmdFields.clear();
	QString url = lines[0].split(" ")[1];
	if (!url.endsWith("/"))
		url.append("/");
	currentCmdFields.insert("url", url);
	foreach (QString line, lines) {
		if (line.contains("user-agent", Qt::CaseInsensitive))
			currentCmdFields.insert("user-agent", line.split(":")[1]);
		if (line.contains("CSeq: ", Qt::CaseInsensitive))
			currentCmdFields.insert("CSeq", line.remove("CSeq: "));
		if (line.contains("Authorization: ", Qt::CaseInsensitive))
			currentCmdFields.insert("Authorization", line.remove("Authorization: "));
	}

	QString urlAction = lines.first().trimmed().split(" ").first();
	if (urlAction == "GET") {
		/* http tunneling */
		QStringList resp;
		resp << "HTTP/1.0 200 OK";
		resp << "LMM RTSP Server 1.0";
		resp << "Cache-Control: no-store";
		resp << "Pragma: no-cache";
		resp << "Content-Type:application/x-rtsp-tunnelled";
		resp << "";
		resp << "";
		lastTunnellingSocket = currentSocket;
		return resp;
	}

	if (auth == AUTH_SIMPLE) {
		if (!currentCmdFields.contains("Authorization"))
			return createRtspErrorResponse(401, lsep);
		QStringList flds = currentCmdFields["Authorization"].split(" ");
		if (flds.size() != 2)
			return createRtspErrorResponse(401, lsep);
		QString cred = QString::fromUtf8(QByteArray::fromBase64(flds[1].toUtf8()));
		QStringList creds = cred.split(":");
		if (creds.size() != 2)
			return createRtspErrorResponse(401, lsep);
		if (creds[0] != authUsername || creds[1] != authPassword)
			return createRtspErrorResponse(401, lsep);
	}

	QMutexLocker l(&sessionLock);
	if (urlAction.startsWith("OPTIONS")) {
		resp = handleCommandOptions(lines, lsep);
	} else if (urlAction.startsWith("DESCRIBE")) {
		resp = handleCommandDescribe(lines, lsep);
	} else if (urlAction.startsWith("SETUP")) {
		resp = handleCommandSetup(lines, lsep);
	} else if (urlAction.startsWith("PLAY")) {
		resp = handleCommandPlay(lines, lsep);
	} else if (urlAction.startsWith("TEARDOWN")) {
		resp = handleCommandTeardown(lines, lsep);
	} else if (urlAction.startsWith("GET_PARAMETER")) {
		resp = handleCommandGetParameter(lines, lsep);
	} else if (urlAction.startsWith("SET_PARAMETER")) {
		resp = handleCommandSetParameter(lines, lsep);
	} else {
		mDebug("Unknown RTSP directive:\n %s", qPrintable(mes));
		return createRtspErrorResponse(501, lsep);
	}

	return resp;
}

void BaseRtspServer::sendRtspMessage(QTcpSocket *sock, const QByteArray &mes)
{
	mInfo("sending rtsp message to %s: %s", qPrintable(sock->peerAddress().toString()),
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
	QString stream = "stream1";
	if (fields.size() > 2)
		stream = fields[2];
	QStringList sdp;
	myIpAddr = findIp(nwInterfaceName);

	/* According to RFC2326 C.1.7 we should report 0.0.0.0 as dest address */
	QString dstIp = "0.0.0.0";
	bool multicast = isMulticast(stream, "");
	int streamPort = 0;
	if (multicast)
		dstIp = getMulticastAddress(stream, "") + "/255";

	sdp << "v=0";
	sdp << "o=- 0 0 IN IP4 127.0.0.1";
	sdp << "c=IN IP4 " + dstIp;
	sdp << "a=tool:lmm 2.0.0";
	sdp << "s=No Name";
	sdp << QString("a=control:rtsp://%1/%2").arg(getEndpointAddress()).arg(stream);

	QHashIterator<QString, StreamDescription> hi(streamDescriptions[stream].media);
	while (hi.hasNext()) {
		hi.next();
		const StreamDescription &desc = hi.value();
		if (multicast)
			streamPort = getMulticastPort(stream, hi.key());
		Lmm::CodecType codec = desc.rtp->getCodec();
		if (codec == Lmm::CODEC_H264) {
			sdp << QString("m=video %1 RTP/AVP 96").arg(streamPort);
			sdp << QString("a=control:rtsp://%1/%2/%3").arg(getEndpointAddress()).arg(stream).arg(desc.streamUrlSuffix);
			sdp << "a=rtpmap:96 H264/90000";
			sdp << "a=fmtp:96 packetization-mode=1";
			//sdp << QString("a=control:%1").arg(desc.streamUrlSuffix);
		} else if (codec == Lmm::CODEC_JPEG) {
			sdp << "m=video 15678 RTP/AVP 26";
		} else if (codec == Lmm::CODEC_PCM_L16) {
			sdp << QString("m=audio %1 RTP/AVP 97").arg(streamPort);
			sdp << QString("a=control:rtsp://%1/%2/%3").arg(getEndpointAddress()).arg(stream).arg(desc.streamUrlSuffix);
			sdp << "a=rtpmap:97 L16/8000/1";
			//sdp << QString("a=control:%1").arg(desc.streamUrlSuffix);
		} else if (codec == Lmm::CODEC_PCM_ALAW) {
			sdp << QString("m=audio %1 RTP/AVP 8").arg(streamPort);
			sdp << QString("a=control:rtsp://%1/%2/%3").arg(getEndpointAddress()).arg(stream).arg(desc.streamUrlSuffix);
			sdp << "a=rtpmap:8 PCMA/8000/1";
		} else if (codec == Lmm::CODEC_META_BILKON) {
			sdp << QString("m=metadata %1 RTP/AVP 98").arg(streamPort);
			sdp << QString("a=control:rtsp://%1/%2/%3").arg(getEndpointAddress()).arg(stream).arg(desc.streamUrlSuffix);
		} else if (codec == Lmm::CODEC_JPEG) {
			sdp << QString("m=video %1 RTP/AVP 26").arg(streamPort);
			sdp << QString("a=control:rtsp://%1/%2/%3").arg(getEndpointAddress()).arg(stream).arg(desc.streamUrlSuffix);
		}
	}

	return sdp;
}

BaseRtspSession::BaseRtspSession(const QString &iface, BaseRtspServer *parent)
	: QObject(parent)
{
	timeout = new MyTime;
	server = parent;
	state = TEARDOWN;
	/* Let's find our IP address */
	myIpAddr = findIp(iface);
	clientCount = 1;
	rtspTimeoutEnabled = false;
	rtpCh = NULL;
	sourceDataPort = sourceControlPort = 0;
}

BaseRtspSession::~BaseRtspSession()
{
}

int BaseRtspSession::setup(bool mcast, int dPort, int cPort, const QString &streamName, const QString &media, const QString &incomingTransportString)
{
	multicast = mcast;
	dataPort = dPort;
	controlPort = cPort;
	rtpAvpTcp = false;

	/* local port detection */
	if (sourceDataPort == 0 || sourceControlPort == 0) {
		while (!BaseRtspServer::detectLocalPorts(myIpAddr, sourceDataPort, sourceControlPort)) ;
	}

	/* for multicast streams, RTCP port should be same for server and clients */
	if (mcast)
		sourceControlPort = cPort;

	/* create the new rtp channel */
	const QHash<QString, QVariant> pars = server->getStreamParameters(streamName, media);
	rtp = server->getSessionTransmitter(streamName, media);
	if (!rtp)
		return 451;
	int maxCnt = INT_MAX;
	if (pars.contains("MaxUnicastStreamCount"))
		maxCnt = pars["MaxUnicastStreamCount"].toInt();
	if (rtp->getChannelCount() >= maxCnt)
		return 453;
	rtpCh = rtp->addChannel();
	if (pars.contains("TrafficShapingEnabled")) {
		bool enabled = pars["TrafficShapingEnabled"].toBool();
		if (enabled) {
			rtpCh->tb = new TokenBucket(rtpCh);
			rtpCh->tb->setPars(pars["TrafficShapingAverage"].toInt() / 8,
					pars["TrafficShapingBurst"].toInt() / 8,
					pars["TrafficShapingDuration"].toInt());
		}
	}
	rtp->setupChannel(rtpCh, streamIp, dataPort, controlPort, sourceDataPort, sourceControlPort, ssrc);
	connect(rtpCh, SIGNAL(goodbyeRecved()), SLOT(rtpGoodbyeRecved()));
	connect(rtpCh, SIGNAL(sessionTimedOut()), SLOT(rtcpTimedOut()));

	if (incomingTransportString.contains("RTP/AVP/TCP")) {
		transportString = QString("Transport: RTP/AVP/TCP;interleaved=0-1");
		rtpAvpTcp = true;
	} else if (multicast) {
		transportString = QString("Transport: RTP/AVP;multicast;destination=%3;port=%1-%2;ttl=%4;mode=play")
				.arg(dataPort).arg(controlPort).arg(streamIp).arg(ttl);
	} else {
		transportString = QString("Transport: RTP/AVP/UDP;unicast;client_port=%1-%2;server_port=%3-%4;ssrc=%5;mode=play")
				.arg(dataPort).arg(controlPort).arg(sourceDataPort)
				.arg(sourceControlPort).arg(ssrc, 0, 16);
	}

	/* create session identifier */
	if (sessionId.isEmpty())
		sessionId = QUuid::createUuid().toString().split("-")[4].remove("}");
	state = SETUP;

	return 0;
}

QString BaseRtspSession::rtpInfo()
{
	return QString("url=rtsp://%1/%2;seq=%4;rtptime=%3").
			arg(server->getEndpointAddress()).
			arg(streamName).arg(rtptime).arg(seq);
}

void BaseRtspSession::rtpGoodbyeRecved()
{
	mDebug("RTCP goodbye received for session '%s'", qPrintable(sessionId));
	server->closeSession(sessionId);
}

void BaseRtspSession::rtcpTimedOut()
{
	if (!rtspTimeoutEnabled) {
		/* we don't use RTSP timeout, so we can close session */
		mDebug("RTCP timed out for session '%s'", qPrintable(sessionId));
		server->closeSession(sessionId);
		return;
	}
	if (timeout->elapsed() < 60000)
		/* we use RTSP timeout and it is not timed-out yet */
		return;
	mDebug("both RTSP and RTCP timed-out, closing session '%s'", qPrintable(sessionId));
	server->closeSession(sessionId);
}

int BaseRtspSession::play()
{
	if (state != PLAY) {
		state = PLAY;
		rtp->playChannel(rtpCh);
	}
	return 0;
}

int BaseRtspSession::teardown()
{
	if (state == PLAY) {
		rtp->teardownChannel(rtpCh);
		rtp->removeChannel(rtpCh);
		rtpCh = NULL;
		state = TEARDOWN;
	}
	return 0;
}
