#ifndef JPEGSHOTSERVER_H
#define JPEGSHOTSERVER_H

#include <ecl/net/simplehttpserver.h>

class BaseStreamer;

class JpegShotServer : public SimpleHttpServer
{
	Q_OBJECT
public:
	explicit JpegShotServer(BaseStreamer *s, int port, QObject *parent = 0);
	QStringList addCustomGetHeaders(const QString &filename);

protected:
	const QByteArray getFile(const QString filename, QString &mime, QUrl &url);
	BaseStreamer *streamer;
};

#endif // JPEGSHOTSERVER_H
