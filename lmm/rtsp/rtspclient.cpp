#include "rtspclient.h"
#include "rtp/rtpreceiver.h"
#include "debug.h"

#include <errno.h>

#include <QUrl>
#include <QTimer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QElapsedTimer>

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

RtspClient::RtspClient(QObject *parent) :
	QObject(parent)
{
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
	timer->start(1000);
	state = UNKNOWN;
}

int RtspClient::setServerUrl(const QString &url)
{
	serverUrl = url;
	cseq = 1;
	return 0;
}

int RtspClient::getOptions()
{
	if (serverUrl.isEmpty())
		return -EINVAL;
	QStringList lines;
	lines << "OPTIONS * RTSP/1.0";
	lines << QString("CSeq: %1").arg(getCSeq());
	lines << "\r\n";
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
	serverInfo.options.clear();
	QStringList options = resp["Public"].split(",");
	foreach (QString option, options)
		serverInfo.options << option.trimmed();
	return 0;
}

int RtspClient::describeUrl()
{
	if (serverUrl.isEmpty())
		return -EINVAL;
	QStringList lines;
	lines << QString("DESCRIBE %1 RTSP/1.0").arg(serverUrl);
	lines << QString("CSeq: %1").arg(getCSeq());
	lines << QString("Accept: application/sdp");
	lines << "\r\n";
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
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

int RtspClient::playSession(const QString &id)
{
	if (serverUrl.isEmpty())
		return -EINVAL;
	QStringList lines;
	lines << QString("PLAY %1 RTSP/1.0").arg(serverUrl);
	lines << QString("CSeq: %1").arg(getCSeq());
	lines << QString("Session: %1").arg(id);
	lines << QString("Range: npt=0.000-");
	lines << "\r\n";
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
	RtspSession ses = setupedSessions[id];
	setupedSessions.remove(id);
	playedSessions.insert(ses.id, ses);
	return keepAlive(id);
}

int RtspClient::teardownSession(const QString &id)
{
	mDebug("%s", qPrintable(id));
	if (serverUrl.isEmpty())
		return -EINVAL;
	if (!playedSessions.contains(id))
		return -ENOENT;
	QStringList lines;
	lines << QString("TEARDOWN %1 RTSP/1.0").arg(serverUrl);
	lines << QString("CSeq: %1").arg(getCSeq());
	lines << QString("Session: %1").arg(id);
	lines << "\r\n";
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
	playedSessions[id].rtp->stop();
	playedSessions.remove(id);
	return 0;
}

int RtspClient::keepAlive(const QString &id)
{
	if (serverUrl.isEmpty())
		return -EINVAL;
	QStringList lines;
	lines << QString("GET_PARAMETER %1 RTSP/1.0").arg(serverUrl);
	lines << QString("CSeq: %1").arg(getCSeq());
	lines << QString("Session: %1").arg(id);
	lines << "\r\n";
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
	playedSessions[id].rtspTimeout.restart();
	return 0;
}

RtpReceiver *RtspClient::getSessionRtp(const QString &id)
{
	if (!playedSessions.contains(id))
		return NULL;
	return playedSessions[id].rtp;
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

	QHashIterator<QString, RtspSession> i(playedSessions);
	while (i.hasNext()) {
		i.next();
		const RtspSession &ses = i.value();
		int stout = ses.pars["timeout"].toInt() * 1000;
		if (stout && ses.rtspTimeout.elapsed() > stout - 5000)
			keepAlive(ses.id);
	}
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
			break;
		} else {
			/* message is splitted in multiple requests */
			msgbuffer = mes;
		}
	}
	if (t.elapsed() > 5000)
		return -ETIMEDOUT;

	return 0;
}

int RtspClient::setupTrack(const QString &controlUrl, const QString &connInfo, RtpReceiver *rtp)
{
	if (!rtp) {
		mDebug("Invalid RTP receiver");
		return -EINVAL;
	}
	QStringList lines;
	lines << QString("SETUP %1 RTSP/1.0").arg(controlUrl);
	lines << QString("CSeq: %1").arg(getCSeq());
	QPair<int, int> p;
	if (!connInfo.contains("239.")) {
		p = detectLocalPorts();
		lines << QString("Transport: RTP/AVP;unicast;client_port=%1-%2").arg(p.first).arg(p.second);
	} else {
		p.first = 15678;
		p.second = 15679;
		lines << QString("Transport: RTP/AVP;multicast;port=%1-%2").arg(p.first).arg(p.second);
	}
	lines << "\r\n";
	QHash<QString, QString> resp;
	int err = waitResponse(lines, resp);
	if (err)
		return err;
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

	ffDebug() << QHostAddress(connInfo.split(" ").last().split("/").first());
	rtp->setSourceAddress(QHostAddress(connInfo.split(" ").last().split("/").first()));
	rtp->setSourceDataPort(p.first);
	rtp->setSourceControlPort(p.second);
	rtp->setDestinationControlPort(getField(resp["Transport"], "server_port").split("-").last().toInt());
	rtp->stop();
	if (rtp->start())
		return -EPERM;
	ses.rtp = rtp;

	setupedSessions.insert(ses.id, ses);

	return 0;
}
