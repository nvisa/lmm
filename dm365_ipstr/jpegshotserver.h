#ifndef JPEGSHOTSERVER_H
#define JPEGSHOTSERVER_H

#include <ecl/net/simplehttpserver.h>
#include <lmm/interfaces/imagesnapshotinterface.h>

class BaseStreamer;

class JpegShotServer : public SimpleHttpServer
{
	Q_OBJECT
public:
	explicit JpegShotServer(ImageSnapshotInterface *s, int port, QObject *parent = 0);
	QStringList addCustomGetHeaders(const QString &filename);

protected:
	const QByteArray getFile(const QString filename, QString &mime, QUrl &url);
	ImageSnapshotInterface *streamer;
};

#endif // JPEGSHOTSERVER_H
