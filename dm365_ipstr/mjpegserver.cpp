#include "mjpegserver.h"
#include "jpegshotserver.h"

#include <lmm/debug.h>
#include <lmm/rawbuffer.h>
#include <lmm/lmmcommon.h>
#include <lmm/players/basestreamer.h>

#include <QTcpSocket>

#include <sys/types.h>
#include <sys/socket.h>

/**
	\class MjpegServer

	\brief This class is our M-JPEG over HTTP implementation. It serves video captured on our encoders
	using a JPEG encoding bitstream encapsulated in HTTP packets. Any recent web browser can be used
	to watch generated video stream.

	MjpegServer class can work in 2 modes: push-based and pull-based operation. In pull-based operation
	it fetches JPEG images from given streamer application in a given frame rate. Let's say we want to
	serve 12.5 fps to our clients using pull-based method, then we create our server as follows:

		new MjpegServer(targetStreamer, 0, 17654, 12.5);

	From this point on our server will listen on port 17654 and when a client is connected, it will serve
	that client with a 12.5 fps M-JPG server.

	Another mode of operation is push-based where another external class pushes buffers randomly to the server. In this
	mode frame-rate is determined external class. One can check MjpegElement implementation for a working example.
*/

/**
 * @brief MjpegServer::MjpegServer
 * @param port
 * @param parent
 */
MjpegServer::MjpegServer(int port, QObject *parent) :
	SimpleHttpServer(port, parent)
{
	streamer = NULL;
	timer = NULL;
}

MjpegServer::MjpegServer(BaseStreamer *s, int channel, int port, float fps, QObject *parent) :
	SimpleHttpServer(port, parent)
{
	streamer = s;
	timer = new QTimer(this);
	shapshotChannel = channel;
	connect(timer, SIGNAL(timeout()), SLOT(timeout()));
	pullPeriod = 1000 / fps;
}

void MjpegServer::timeout()
{
	QList<RawBuffer> snapshots = streamer->getSnapshot(shapshotChannel, Lmm::CODEC_JPEG, 0, 1);
	for (int i = 0; i < snapshots.size(); i++) {
		RawBuffer buf = snapshots[i];
		transmitImage(buf);
	}
}

void MjpegServer::clientDisconnected()
{
	QTcpSocket *sock = (QTcpSocket *)sender();
	lock.lock();
	clients.removeAll(sock);
	lock.unlock();
}

void MjpegServer::sendGetResponse(QTcpSocket *sock)
{
	connect(sock, SIGNAL(disconnected()), SLOT(clientDisconnected()));
	lock.lock();
	clients << sock;

	QByteArray resp;
	resp.append(QString("HTTP/1.1 200 OK").toUtf8());
	resp.append(QString("Cache-Control: no-cache, no-store, must-revalidate\r\n").toUtf8());
	resp.append(QString("Pragma: no-cache\r\n").toUtf8());
	resp.append(QString("Expires: 0\r\n").toUtf8());
	resp.append(QString("Content-Type: multipart/x-mixed-replace; boundary=someboundrydefinition\r\n").toUtf8());
	resp.append(QString("\r\n").toUtf8());
	sock->write(resp);

	lock.unlock();

	/* start timer if we are working in pull-based operation */
	if (timer)
		timer->start(pullPeriod);
}

void MjpegServer::transmitImage(const RawBuffer &buf)
{
	lock.lock();

	/* optimization: do nothing if no one is connected */
	if (!clients.size()) {
		lock.unlock();
		return;
	}

	/* prepare message header */
	QByteArray resp;
	resp.append(QString("--someboundrydefinition\r\n").toUtf8());
	resp.append(QString("Content-Type: image/jpeg\r\n").toUtf8());
	resp.append(QString("Content-Length: %1\r\n").arg(buf.size()).toUtf8());
	resp.append(QString("\r\n").toUtf8());

	/* now send message to all connected clients */
	foreach (QTcpSocket *sock, clients) {
		/*
		 * trick: use native API's as QTcpSocket is not multi-thread friendly (for Qt 4.6.3)
		 *
		 * Normally we would do this:
		 *
		 *		sock->write(resp);
		 *		sock->write((const char *)buf.constData(), buf.size());
		 *
		 * However this causes some memory leak so we use write()/send() native API functions
		 */
		int sd = sock->socketDescriptor();

		/* send header */
		send(sd, resp.constData(), resp.size(), 0);

		/* now the image itself */
		const char *data = (const char *)buf.constData();
		int left = buf.size();
		while (left) {
			int len = send(sd, data, left, 0);
			if (len == -1)
				break;
			data += len;
			left -= len;
		}
	}

	lock.unlock();
}

int MjpegServer::getClientCount()
{
	lock.lock();
	int cnt = clients.count();
	lock.unlock();
	return cnt;
}

MjpegElement::MjpegElement(int port, QObject *parent) :
	BaseLmmElement(parent)
{
	server = new MjpegServer(port, this);
	jpegServer = new JpegShotServer(this, 4571, this);
}

bool MjpegElement::isActive()
{
	return server->getClientCount() ? true : jpegWaiting.available();
}

QList<RawBuffer> MjpegElement::getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount)
{
	Q_UNUSED(ch);
	Q_UNUSED(codec);
	Q_UNUSED(ts);
	jpegBufList.clear();
	jpegWaiting.release(frameCount);
	jpegSem.acquire(frameCount);
	return jpegBufList;
}

int MjpegElement::processBuffer(const RawBuffer &buf)
{
	if (server->getClientCount())
		server->transmitImage(buf);

	if (jpegWaiting.available()) {
		jpegWaiting.acquire(1);
		jpegBufList << buf;
		jpegSem.release(1);
	} else
		jpegBufList.clear();

	return newOutputBuffer(0, buf);
}
