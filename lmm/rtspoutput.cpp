#include "rtspoutput.h"
#include "baselmmoutput.h"
#include "gstreamer/rtpstreamer.h"

#include <emdesk/debug.h>

#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalMapper>
#include <QStringList>
#include <QEventLoop>

RtspOutput::RtspOutput(QObject *parent) :
	BaseLmmOutput(parent)
{
	gstRtp = new RtpStreamer;
	server = new QTcpServer(this);
	server->listen(QHostAddress::Any, 554);
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
	gstRtp->stopPlayback();
	return BaseLmmOutput::stop();
}

int RtspOutput::outputBuffer(RawBuffer buf)
{
	if (!gstRtp->isRunning())
		return -1;
	mInfo("pushing new buffer with size %d", buf.size());
	gstRtp->sendBuffer(buf);
	return 0;
}

int RtspOutput::getOutputBufferCount()
{
	return BaseLmmElement::getOutputBufferCount() + gstRtp->getWaitingBufferCount();
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
}

void RtspOutput::clientError(QObject *obj)
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
	if (mes.contains("\r\n"))
		lsep = "\r\n";
	else if (mes.contains("\n\r"))
		lsep = "\n\r";
	else if (mes.contains("\n"))
		lsep = "\n";
	else if (mes.contains("\r"))
		lsep = "\r";
	QString end = lsep + lsep;
	if (mes.contains(end)) {
		qDebug() << "new message from rtsp client: " << mes;
#if 1
		QStringList resp = handleRtspMessage(mes, lsep);
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

QStringList RtspOutput::handleRtspMessage(QString mes, QString lsep)
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
		resp << "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE";
		resp << lsep;
	} else if (lines.first().startsWith("DESCRIBE")) {
		mDebug("handling describe directive");
		QString cbase = lines[0].split(" ")[1];
		int sdpSize = lsep.length(); //includes last seperator
		QStringList sdp = createSdp();
		foreach (const QString &sdpline, sdp)
			sdpSize += sdpline.length();
		int cseq = lines[1].remove("CSeq: ").toInt();
		resp << "RTSP/1.0 200 OK";
		resp << "Content-type: application/sdp";
		resp << "Cache-Control: no-cache";
		resp << QString("Content-Length: %1").arg(sdpSize);
		resp << QString("Content-Base: %1").arg(cbase);
		resp << QString("CSeq: %1").arg(cseq);
		resp << lsep;
		resp << sdp;
		resp << lsep;
	} else if (lines.first().startsWith("SETUP")) {
		mDebug("handling setup directive");
		//QString cbase = lines[0].split(" ")[1];
		int cseq = lines[1].remove("CSeq: ").toInt();
		int dataPort, controlPort;
		foreach(QString line, lines) {
			if (line.contains("Transport:")) {
				QStringList fields = line.remove("Transport:").split(";");
				foreach(QString field, fields) {
					if (field.contains("client_port")) {
						/* TODO: check field sizes */
						dataPort = field.split("=")[1].split("-")[0].toInt();
						controlPort = field.split("=")[1].split("-")[1].toInt();
					}
				}
			}
		}

		gstRtp->setDestinationDataPort(dataPort);
		gstRtp->setDestinationControlPort(controlPort);

		resp << "RTSP/1.0 200 OK";
		resp << QString("Transport: RTP/AVP/UDP;unicast;client_port=%1-%2;server_port=%3-%4;ssrc=6335D514;mode=play")
				.arg(dataPort).arg(controlPort).arg(gstRtp->getSourceDataPort()).arg(gstRtp->getSourceControlPort());
		resp << "Session: cbbb605922959838";
		resp << "Content-Length: 0";
		resp << "Cache-Control: no-cache";
		resp << QString("CSeq: %1").arg(cseq);
		resp << lsep;


		//gstRtp->setSourceDataPort(31478);
		//gstRtp->setSourceControlPort(31479);
	} else if (lines.first().startsWith("PLAY")) {
		mDebug("handling play directive");
		int cseq = lines[1].remove("CSeq: ").toInt();
		resp << "RTSP/1.0 200 OK";
		resp << "RTP-Info: url=rtsp://192.168.1.196:554/cam264.sdp/trackID=0;seq=6666;rtptime=1578998879";
		resp << "Content-Length: 0";
		resp << "Cache-Control: no-cache";
		resp << QString("CSeq: %1").arg(cseq);
		resp << lsep;

		gstRtp->setRtpSequenceOffset(6666);
		gstRtp->setRtpTimestampOffset(1578998879);
		gstRtp->startPlayback();
	} else if (lines.first().startsWith("TEARDOWN")) {
		mDebug("handling teardown directive");
		int cseq = lines[1].remove("CSeq: ").toInt();
		resp << "RTSP/1.0 200 OK";
		resp << "Session: cbbb605922959838";
		resp << "Content-Length: 0";
		resp << "Cache-Control: no-cache";
		resp << QString("CSeq: %1").arg(cseq);
		resp << lsep;

		gstRtp->stopPlayback();
	} else
		mDebug("Unknown RTSP directive:\n %s", qPrintable(mes));

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

QStringList RtspOutput::createSdp()
{
	QStringList sdp;
	sdp << "m=video 0 RTP/AVP 96";
	sdp << "a=rtpmap:96 H264/90000";
	sdp << "a=fmtp:96 packetization-mode=1";
	sdp << "a=mimetype:string;\"video/h264\"";
	sdp << "a=control:rtsp://192.168.1.196:554/cam264.sdp/trackID=0";
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
