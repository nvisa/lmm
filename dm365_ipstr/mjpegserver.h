#ifndef MJPEGSERVER_H
#define MJPEGSERVER_H

#include <lmm/rawbuffer.h>
#include <lmm/baselmmelement.h>

#include <ecl/net/simplehttpserver.h>

#include <QTimer>
#include <QStringList>

class QTimer;
class BaseStreamer;

class MjpegServer : public SimpleHttpServer
{
	Q_OBJECT
public:
	explicit MjpegServer(int port, QObject *parent = 0);
	explicit MjpegServer(BaseStreamer *s, int channel, int port, float fps, QObject *parent = 0);

	void transmitImage(const RawBuffer &buf);
protected slots:
	void timeout();
	void clientDisconnected();

protected:
	virtual void sendGetResponse(QTcpSocket *sock);

	BaseStreamer *streamer;
	int shapshotChannel;
	QTimer *timer;
	QList<QTcpSocket *> clients;
	QMutex lock;
	int pullPeriod;
};

class MjpegElement : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit MjpegElement(int port, QObject *parent = 0);

protected:
	int processBuffer(const RawBuffer &buf);

	MjpegServer *server;
};

#endif // MJPEGSERVER_H
