#include "rtspclient.h"
#include "rtp/rtpreceiver.h"
#include "debug.h"

#include <errno.h>

#include <QUrl>
#include <QTimer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QElapsedTimer>

static QHostAddress getConnectionAddress(const QString &conninfo)
{
	QStringList flds = conninfo.split(" ", QString::SkipEmptyParts);
	foreach (const QString &fld, flds) {
		if (fld.count(".") != 3)
			continue;
		qDebug() << fld << fld.count(".") << QHostAddress::parseSubnet(fld);
		if (fld.contains("/"))
			return QHostAddress(fld.split("/").first());
		return QHostAddress(fld);
	}
	return QHostAddress();
}

static QPair<int, int> detectLocalPorts()
{
	QUdpSocket sock;
	sock.bind(0);
	int port = sock.localPort();
	sock.abort();
	sock.close();
	int port2 = (port & 0x1) ? port - 1 : port + 1;
	if (sock.bind(port2)) {
		sock.abort();
		sock.close();
		if (port < port2)
			return QPair<int, int>(port, port2);
		return QPair<int, int>(port2, port);
	}
	return detectLocalPorts();
}

static QString getField(const QString &line, const QString &field)
{
	QStringList flds = line.split(";");
	foreach (QString fld, flds) {
		QStringList pars = fld.split("=");
		if (pars.size() < 2)
			continue;
		if (pars[0] == field)
			return pars[1];
	}
	return "";
}

static QStringList createOptionsReq(int cseq)
{
	QStringList lines;
	lines << "OPTIONS * RTSP/1.0";
	lines << QString("CSeq: %1").arg(cseq);
	lines << "\r\n";
	return lines;
}

static QStringList createDescribeReq(int cseq, QString serverUrl)
{
	QStringList lines;
	lines << QString("DESCRIBE %1 RTSP/1.0").arg(serverUrl);
	lines << QString("CSeq: %1").arg(cseq);
	lines << QString("Accept: application/sdp");
	lines << "\r\n";
	return lines;
}

static QStringList createSetupReq(int cseq, QString controlUrl, QString connInfo, QPair<int, int> &p)
{
	QStringList lines;
	lines << QString("SETUP %1 RTSP/1.0").arg(controlUrl);
	lines << QString("CSeq: %1").arg(cseq);
	p = detectLocalPorts();
	if (!getConnectionAddress(connInfo).isInSubnet(QHostAddress::parseSubnet("224.0.0.0/3")))
		lines << QString("Transport: RTP/AVP;unicast;client_port=%1-%2").arg(p.first).arg(p.second);
	else
		lines << QString("Transport: RTP/AVP;multicast;port=%1-%2").arg(p.first).arg(p.second);
	lines << "\r\n";
	return lines;
}

static QStringList createPlayReq(int cseq, QString serverUrl, QString id)
{
	QStringList lines;
	lines << QString("PLAY %1 RTSP/1.0").arg(serverUrl);
	lines << QString("CSeq: %1").arg(cseq);
	lines << QString("Session: %1").arg(id);
	lines << QString("Range: npt=0.000-");
	lines << "\r\n";
	return lines;
}

static QStringList createTeardownReq(int cseq, QString serverUrl, QString id)
{
	QStringList lines;
	lines << QString("TEARDOWN %1 RTSP/1.0").arg(serverUrl);
	lines << QString("CSeq: %1").arg(cseq);
	lines << QString("Session: %1").arg(id);
	lines << "\r\n";
	return lines;
}

static QStringList createKeepAliveReq(int cseq, QString serverUrl, QString id)
{
	QStringList lines;
	lines << QString("GET_PARAMETER %1 RTSP/1.0").arg(serverUrl);
	lines << QString("CSeq: %1").arg(cseq);
	lines << QString("Session: %1").arg(id);
	lines << "\r\n";
	return lines;
}

RtspClient::RtspClient(QObject *parent) :
	QObject(parent)
{
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
	state = UNKNOWN;
	asyncPlay = true;
	asyncsock = NULL;
}

int RtspClient::setServerUrl(const QString &url)
{
	serverUrl = url;
	cseq = 1;
	asyncsock = new QTcpSocket(this);
	connect(asyncsock, SIGNAL(readyRead()), SLOT(aSyncDataReady()));
	connect(asyncsock, SIGNAL(connected()), SLOT(aSyncConnected()));
	connect(asyncsock, SIGNAL(disconnected()), SLOT(aSyncDisConnected()));
	devstatus = DEAD;

	QUrl rurl(serverUrl);
	int port = rurl.port();
	if (port < 0)
		port = 554;
	asyncsock->connectToHost(rurl.host(), port);

	timer->start(100);

	return 0;
}

int RtspClient::getOptions()
{
	if (serverUrl.isEmpty())
		return -EINVAL;
	serverInfo.options.clear();
	QStringList lines = createOptionsReq(getCSeq());
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
	return parseOptionsResponse(resp);

}

int RtspClient::getOptionsASync()
{
	if (devstatus != ALIVE)
		return -EINVAL;
	int cseq = getCSeq();
	QStringList lines = createOptionsReq(cseq);
	cseqRequests.insert(cseq, CSeqRequest("OPTIONS"));
	asyncsock->write(lines.join("\r\n").toUtf8());
	return 0;
}

int RtspClient::describeUrl()
{
	if (serverUrl.isEmpty())
		return -EINVAL;
	QStringList lines = createDescribeReq(getCSeq(), serverUrl);
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
	return parseDescribeResponse(resp);
}

int RtspClient::describeUrlASync()
{
	if (devstatus != ALIVE)
		return -EINVAL;
	int cseq = getCSeq();
	QStringList lines = createDescribeReq(cseq, serverUrl);
	cseqRequests.insert(cseq, CSeqRequest("DESCRIBE"));
	asyncsock->write(lines.join("\r\n").toUtf8());
	return 0;
}

int RtspClient::setupUrl()
{
	if (serverUrl.isEmpty())
		return -EINVAL;

	int err = 0;
	bool tracksUp = false;
	if (setupTracks.size()) {
		const QList<TrackDefinition> &tracks = serverDescriptions[serverUrl];
		for (int i = 0; i < tracks.size(); i++) {
			if (tracks[i].controlUrl == serverUrl)
				continue;
			QString controlUrl = tracks[i].controlUrl;
			err = setupTrack(controlUrl, tracks[i].connection, trackReceivers[tracks[i].name]);
			if (err) {
				mDebug("error %d setting-up session %s", err, qPrintable(tracks[i].name));
				return err;
			}
			tracksUp = true;
		}
	}

	if (!tracksUp)
		/* following will return error in any case due to NULL RtpReceiver */
		return setupTrack(serverUrl, "", NULL);

	return 0;
}

int RtspClient::setupUrlASync()
{
	if (devstatus != ALIVE)
		return -EINVAL;

	int err = 0;
	bool tracksUp = false;
	if (setupTracks.size()) {
		const QList<TrackDefinition> &tracks = serverDescriptions[serverUrl];
		for (int i = 0; i < tracks.size(); i++) {
			if (tracks[i].controlUrl == serverUrl)
				continue;
			QString controlUrl = tracks[i].controlUrl;
			err = setupTrackASync(controlUrl, tracks[i].connection, trackReceivers[tracks[i].name]);
			if (err) {
				mDebug("error %d setting-up session %s", err, qPrintable(tracks[i].name));
				return err;
			}
			tracksUp = true;
		}
	}

	if (!tracksUp)
		/* following will return error in any case due to NULL RtpReceiver */
		return setupTrackASync(serverUrl, "", NULL);

	return 0;
}

int RtspClient::playSession(const QString &id)
{
	if (serverUrl.isEmpty())
		return -EINVAL;
	QStringList lines = createPlayReq(getCSeq(), serverUrl, id);
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
	parsePlayResponse(resp, id);
	return keepAlive(id);
}

int RtspClient::playSessionASync(const QString &id)
{
	if (devstatus != ALIVE)
		return -EINVAL;
	int cseq = getCSeq();
	QStringList lines = createPlayReq(cseq, serverUrl, id);
	CSeqRequest req("PLAY");
	req.id = id;
	cseqRequests.insert(cseq, req);
	asyncsock->write(lines.join("\r\n").toUtf8());
	return 0;
}

int RtspClient::teardownSession(const QString &id)
{
	mDebug("%s", qPrintable(id));
	if (serverUrl.isEmpty())
		return -EINVAL;
	if (!playedSessions.contains(id))
		return -ENOENT;
	QStringList lines = createTeardownReq(getCSeq(), serverUrl, id);
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
	return parseTeardownResponse(resp, id);
}

int RtspClient::teardownSessionASync(const QString &id)
{
	if (devstatus != ALIVE)
		return -EINVAL;
	int cseq = getCSeq();
	QStringList lines = createTeardownReq(cseq, serverUrl, id);
	CSeqRequest req("TEARDOWN");
	req.id = id;
	cseqRequests.insert(cseq, req);
	asyncsock->write(lines.join("\r\n").toUtf8());
	return 0;
}

int RtspClient::keepAlive(const QString &id)
{
	if (serverUrl.isEmpty())
		return -EINVAL;
	QStringList lines = createKeepAliveReq(getCSeq(), serverUrl, id);
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
	return parseKeepAliveResponse(resp, id);
}

int RtspClient::keepAliveASync(const QString &id)
{
	if (devstatus != ALIVE)
		return -EINVAL;
	int cseq = getCSeq();
	QStringList lines = createKeepAliveReq(cseq, serverUrl, id);
	CSeqRequest req("GET_PARAMETER");
	req.id = id;
	cseqRequests.insert(cseq, req);
	asyncsock->write(lines.join("\r\n").toUtf8());
	return 0;
}

RtpReceiver *RtspClient::getSessionRtp(const QString &id)
{
	if (!playedSessions.contains(id))
		return NULL;
	return playedSessions[id].rtp;
}

void RtspClient::playASync()
{
	asyncPlay = true;
}

void RtspClient::teardownASync()
{
	asyncPlay = false;
}

void RtspClient::addSetupTrack(const QString &name, RtpReceiver *rtp)
{
	setupTracks << name;
	trackReceivers.insert(name, rtp);
}

void RtspClient::clearSetupTracks()
{
	setupTracks.clear();
	trackReceivers.clear();
}

void RtspClient::timeout()
{
	if (!asyncsock)
		return;
	mInfo("state=%d", state);
#if 0
	switch (state) {
	case UNKNOWN:
		getOptions();
		if (!describeUrl())
			state = DESCRIBED;
		break;
	case DESCRIBED:
		if (!setupUrl())
			state = SETTEDUP;
		break;
	case SETTEDUP:
		foreach (const QString &ses, setupedSessions.keys()) {
			int err = playSession(ses);
			if (err)
				mDebug("Error '%d' playing session '%s'", err, qPrintable(ses));
		}
		if (setupedSessions.size() == 0 && playedSessions.size())
			state = PLAYED;
		break;
	case PLAYED:
		/* transition state, nothing to do */
		state = STREAMING;
		break;
	case STREAMING:
		break;
	}
#else
	switch (state) {
	case UNKNOWN:
		if (!getOptionsASync())
			state = OPTIONS_WAIT;
		break;
	case OPTIONS_WAIT:
		if (serverInfo.options.size()) {
			if (!describeUrlASync())
				state = DESCRIBE_WAIT;
		}
		break;
	case DESCRIBE_WAIT:
		if (serverDescriptions.contains(serverUrl)) {
			state = DESCRIBED;
		}
		break;
	case SETUP_WAIT: {
		const QList<TrackDefinition> &tracks = serverDescriptions[serverUrl];
		if (tracks.size() == settedUpSessions().size()) {
			int count = 0;
			foreach (const QString &ses, setupedSessions.keys()) {
				if (!playSessionASync(ses))
					count++;
			}
			if (count == setupedSessions.size())
				state = PLAY_WAIT;
		}
		break;
	}
	case PLAY_WAIT:
		if (setupedSessions.size() == 0 && playedSessions.size())
			state = PLAYED;
		break;
	case DESCRIBED:
		if (asyncPlay) {
			if (!setupUrlASync())
				state = SETUP_WAIT;
		}
		break;
	case SETTEDUP:
		foreach (const QString &ses, setupedSessions.keys()) {
			int err = playSession(ses);
			if (err)
				mDebug("Error '%d' playing session '%s'", err, qPrintable(ses));
		}
		if (setupedSessions.size() == 0 && playedSessions.size())
			state = PLAYED;
		break;
	case PLAYED:
		/* transition state, nothing to do */
		state = STREAMING;
		break;
	case STREAMING:
		if (!asyncPlay) {
			int count = 0;
			foreach (const QString &ses, playedSessions.keys()) {
				if (!teardownSessionASync(ses))
					count++;
			}
			if (count == playedSessions.size())
				state = DESCRIBED;
		}
		break;
	}
#endif

	QHashIterator<QString, RtspSession> i(playedSessions);
	while (i.hasNext()) {
		i.next();
		const RtspSession &ses = i.value();
		int stout = ses.pars["timeout"].toInt() * 1000;
		if (stout && ses.rtspTimeout.elapsed() > stout - 5000)
			keepAlive(ses.id);
	}
}

void RtspClient::aSyncDataReady()
{
	/* RTSP protocol uses UTF-8 */
	QString str = QString::fromUtf8(asyncsock->readAll());
	if (readResponse(str, currentResp)) {
		int cseq = currentResp["CSeq"].toInt();
		mInfo("async resp %d ready", cseq);
		if (!cseqRequests.contains(cseq)) {
			mDebug("invalid response request received:\n%s", qPrintable(currentResp["__content__"]));
			return;
		}
		CSeqRequest req = cseqRequests[cseq];
		mDebug("Parsing request %s from %s", qPrintable(req.type), qPrintable(asyncsock->peerAddress().toString()));
		if (req.type == "OPTIONS")
			parseOptionsResponse(currentResp);
		else if (req.type == "DESCRIBE")
			parseDescribeResponse(currentResp);
		else if (req.type == "SETUP")
			parseSetupResponse(currentResp, req.rtp, req.p);
		else if (req.type == "PLAY")
			parsePlayResponse(currentResp, req.id);
		else if (req.type == "TEARDOWN")
			parseTeardownResponse(currentResp, req.id);
		else if (req.type == "GET_PARAMETER")
			parseKeepAliveResponse(currentResp, req.id);
		else
			ffDebug() << "buggy request type" << req.type;
	}
}

void RtspClient::aSyncConnected()
{
	mDebug("device is alive");
	devstatus = ALIVE;
}

void RtspClient::aSyncDisConnected()
{
	mDebug("device is dead");
	devstatus = DEAD;
}

int RtspClient::getCSeq()
{
	return cseq++;
}

int RtspClient::waitResponse(const QStringList &lines, QHash<QString, QString> &resp)
{
	QUrl url(serverUrl);
	QTcpSocket sock(this);
	int port = url.port();
	if (port < 0)
		port = 554;
	sock.connectToHost(url.host(), port);
	if (!sock.waitForConnected(5000))
		return -ETIMEDOUT;
	sock.write(lines.join("\r\n").toUtf8());

	QElapsedTimer t;
	t.start();
	while (t.elapsed() < 5000) {
		sock.waitForReadyRead(5000);
		/* RTSP protocol uses UTF-8 */
		QString str = QString::fromUtf8(sock.readAll());
		if (readResponse(str, resp))
			break;
	}
	if (t.elapsed() > 5000)
		return -ETIMEDOUT;

	return 0;
}

bool RtspClient::readResponse(const QString &str, QHash<QString, QString> &resp)
{
	QString mes = msgbuffer.append(str);
	/* RTSP line sepeators should be decided by clients */
	QString lsep = "\r\n";
	/* end of message is 2 line seperators, by the standard */
	QString end = lsep + lsep;
	if (mes.contains(end)) {
		QStringList lines = mes.split("\r\n");
		QStringList content;
		bool inContent = false;
		foreach (const QString &line, lines) {
			if (line.isEmpty()) {
				/* this can be content */
				if (resp.contains("Content-Length"))
					inContent = true;
			}
			if (inContent) {
				content << line;
			} else {
				QStringList flds = line.split(":");
				QString key = flds[0];
				flds.removeFirst();
				resp.insert(key, flds.join(":"));
			}
		}
		if (content.size())
			resp.insert("__content__", content.join("\n"));
		msgbuffer = "";
		return true;
	} else {
		/* message is splitted in multiple requests */
		msgbuffer = mes;
	}

	return false;
}

int RtspClient::setupTrack(const QString &controlUrl, const QString &connInfo, RtpReceiver *rtp)
{
	if (!rtp) {
		mDebug("Invalid RTP receiver");
		return -EINVAL;
	}
	QPair<int, int> p;
	QStringList lines = createSetupReq(getCSeq(), controlUrl, connInfo, p);
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
	return parseSetupResponse(resp, rtp, p);

	return 0;
}

int RtspClient::setupTrackASync(const QString &controlUrl, const QString &connInfo, RtpReceiver *rtp)
{
	if (devstatus != ALIVE)
		return -EINVAL;
	QPair<int, int> p;
	int cseq = getCSeq();
	QStringList lines = createSetupReq(cseq, controlUrl, connInfo, p);
	CSeqRequest req("SETUP");
	req.rtp = rtp;
	req.p = p;
	cseqRequests.insert(cseq, req);
	asyncsock->write(lines.join("\r\n").toUtf8());
	return 0;
}

int RtspClient::parseOptionsResponse(const QHash<QString, QString> &resp)
{
	QStringList options = resp["Public"].split(",");
	foreach (QString option, options)
		serverInfo.options << option.trimmed();
	return 0;
}

int RtspClient::parseDescribeResponse(const QHash<QString, QString> &resp)
{
	const QStringList &sdplines = resp["__content__"].split("\n");
	TrackDefinition tr;
	foreach (QString line, sdplines) {
		if (line.startsWith("m="))
			tr.m = line.remove("m=").trimmed();
		else if (line.startsWith("a=rtpmap"))
			tr.rtpmap = line.remove("a=").trimmed();
		else if (line.startsWith("a=control:")) {
			tr.controlUrl = line.remove("a=control:").trimmed();
			if (!tr.controlUrl.startsWith("rtsp://"))
				tr.controlUrl = QString("%1/%2").arg(serverUrl).arg(tr.controlUrl);

			if (tr.m.startsWith("video"))
				tr.type = "video";
			else if (tr.m.startsWith("audio"))
				tr.type = "audio";
			else if (tr.m.startsWith("metadata"))
				tr.type = "metadata";
		} else if (line.startsWith("c=")) {
			tr.connection = line.remove("c=").trimmed();
		}
	}
	tr.name = tr.controlUrl.split("/").last();
	serverDescriptions[serverUrl] << tr;

	return 0;
}

int RtspClient::parseKeepAliveResponse(const QHash<QString, QString> &resp, const QString &id)
{
	Q_UNUSED(resp);
	playedSessions[id].rtspTimeout.restart();
	return 0;
}

int RtspClient::parsePlayResponse(const QHash<QString, QString> &resp, const QString &id)
{
	Q_UNUSED(resp);
	RtspSession ses = setupedSessions[id];
	setupedSessions.remove(id);
	playedSessions.insert(ses.id, ses);
	return 0;
}

int RtspClient::parseTeardownResponse(const QHash<QString, QString> &resp, const QString &id)
{
	Q_UNUSED(resp);
	playedSessions[id].rtp->stop();
	playedSessions.remove(id);
	return 0;
}

int RtspClient::parseSetupResponse(const QHash<QString, QString> &resp, RtpReceiver *rtp, QPair<int, int> p)
{
	QStringList session = resp["Session"].split(";");
	RtspSession ses;
	foreach (QString s, session) {
		if (s.contains("=")) {
			QStringList flds = s.split("=");
			ses.pars.insert(flds.first(), flds.last());
			continue;
		}
		ses.id = s;
	}

	QUrl url(serverUrl);
	rtp->setSourceDataPort(p.first);
	rtp->setSourceControlPort(p.second);
	QString transport = resp["Transport"];
	if (transport.contains("multicast")) {
		rtp->setDestinationControlPort(p.second);
		rtp->setSourceAddress(QHostAddress(getField(transport, "destination")));
	} else {
		rtp->setDestinationControlPort(getField(transport, "server_port").split("-").last().toInt());
		rtp->setSourceAddress(QHostAddress(url.host()));
	}
	rtp->stop();
	int err = rtp->start();
	if (err) {
		mDebug("error '%d' starting RTP session for '%s'", err, qPrintable(serverUrl));
		return -EPERM;
	}
	ses.rtp = rtp;

	setupedSessions.insert(ses.id, ses);

	return 0;
}
