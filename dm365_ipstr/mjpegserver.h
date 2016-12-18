#ifndef MJPEGSERVER_H
#define MJPEGSERVER_H

#include <lmm/rawbuffer.h>
#include <lmm/baselmmelement.h>
#include <lmm/interfaces/imagesnapshotinterface.h>
#include <lmm/interfaces/streamcontrolelementinterface.h>

#include <ecl/net/simplehttpserver.h>

#include <QTimer>
#include <QSemaphore>
#include <QStringList>

class QTimer;
class BaseStreamer;
class JpegShotServer;

class MjpegServer : public SimpleHttpServer
{
	Q_OBJECT
public:
	explicit MjpegServer(int port, QObject *parent = 0);
	explicit MjpegServer(BaseStreamer *s, int channel, int port, float fps, QObject *parent = 0);

	void transmitImage(const RawBuffer &buf);
	int getClientCount();
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

class MjpegElement : public BaseLmmElement, public StreamControlElementInterface, public ImageSnapshotInterface
{
	Q_OBJECT
public:
	explicit MjpegElement(int port, QObject *parent = 0);
	bool isActive();
	QList<RawBuffer> getSnapshot(int ch, Lmm::CodecType codec, qint64 ts, int frameCount);

protected:
	int processBuffer(const RawBuffer &buf);

	MjpegServer *server;
	JpegShotServer *jpegServer;
	QSemaphore jpegSem;
	QSemaphore jpegWaiting;
	QList<RawBuffer> jpegBufList;
};

#endif // MJPEGSERVER_H
