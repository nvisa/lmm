#include "jpegshotserver.h"
#include "simplezip.h"

#include <lmm/lmmcommon.h>
#include <lmm/rawbuffer.h>
#include <lmm/players/basestreamer.h>

#include <ecl/debug.h>

#include <QStringList>

JpegShotServer::JpegShotServer(ImageSnapshotInterface *s, int port, QObject *parent) :
	SimpleHttpServer(port, parent),
	streamer(s)
{
}

QStringList JpegShotServer::addCustomGetHeaders(const QString &filename)
{
	QStringList headers;
	QString str = filename;
	if (str.endsWith(".zip") || str.endsWith(".bin"))
		headers.append(QString("Content-Disposition: attachment; filename=%1").arg(str.remove("/")));
	return headers;
}

const QByteArray JpegShotServer::getFile(const QString filename, QString &mime, QUrl &url)
{
	QString reqtime = QString("request_time: %1").arg(QDateTime::currentDateTime().toString("hh.mm.ss.zzz"));
	mime = "image/jpeg";
	if (filename.endsWith(".zip"))
		mime = "application/zip";
	if (filename.endsWith(".raw"))
		mime = "application/octet-stream";
	qint64 ts = 0;
	if (url.hasQueryItem("ts"))
		ts = url.queryItemValue("ts").toLongLong();
	int frameCnt = 1;
	if (url.hasQueryItem("fcnt"))
		frameCnt = url.queryItemValue("fcnt").toInt();
	Lmm::CodecType codec = Lmm::CODEC_JPEG;
	if (filename.endsWith(".raw"))
		codec = Lmm::CODEC_RAW;
	if (filename.endsWith(".h264"))
		codec = Lmm::CODEC_H264;
	if (url.hasQueryItem("codec") && url.queryItemValue("codec") == "raw")
		codec = Lmm::CODEC_RAW;
	if (url.hasQueryItem("codec") && url.queryItemValue("codec") == "h264")
		codec = Lmm::CODEC_H264;
	if (url.hasQueryItem("codec") && url.queryItemValue("codec") == "jpeg")
		codec = Lmm::CODEC_JPEG;
	int ch = 0;
	if (filename.contains("snapshot1"))
		ch = 1;
	if (ch == 1)
		frameCnt = 1;
	QList<RawBuffer> snapshots = streamer->getSnapshot(ch, codec, ts, frameCnt);
	if (filename.endsWith(".zip")) {
		QStringList infoLines;
		SimpleZip zip;
		zip.addDirectory("snapshot_images/");
		qint64 last = 0;
		for (int i = 0; i < snapshots.size(); i++) {
			const RawBuffer &buf = snapshots[i];
			if (i == 0)
				last = buf.constPars()->captureTime;
			const QByteArray ba = QByteArray::fromRawData((const char *)buf.constData(), buf.size());
			QString fsuffix = "raw";
			if (codec == Lmm::CODEC_JPEG)
				fsuffix = "jpg";
			if (codec == Lmm::CODEC_H264)
				fsuffix = "h264";
			zip.addFile(QString("snapshot_images/snapshot%1_%2_%4_%5_%6.%3")
						.arg(i + 1).arg(buf.constPars()->streamBufferNo)
						.arg(fsuffix)
						.arg(buf.constPars()->captureTime)
						.arg(buf.constPars()->captureTime - last)
						.arg(buf.constPars()->frameType)
						, ba);
			last = buf.constPars()->captureTime;
			int secs = last / 1000 / 1000;
			int msecs = (last / 1000) % 1000;
			int esecs = buf.constPars()->encodeTime;
			int emsecs = (buf.constPars()->encodeTime / 1000) % 1000;
			infoLines << QString("frame%1: ts=%2 time=%3 msecs=%4 size=%5 no=%6 etime=%7 etime_msecs=%8")
					 .arg(i)
					 .arg(buf.constPars()->captureTime)
					 .arg(QDateTime::fromTime_t(secs).toString("hh.mm.ss"))
					 .arg(msecs)
					 .arg(buf.size())
					 .arg(buf.constPars()->streamBufferNo)
					 .arg(QDateTime::fromTime_t(esecs).toString("hh.mm.ss"))
					 .arg(emsecs);
		}
		infoLines << "";
		infoLines << reqtime;
		infoLines << url.toString();
		infoLines << QString("PC ts: %1").arg(ts);
		infoLines << QString("PC ts decoded: %1.%2").arg(QDateTime::fromTime_t(ts / 1000 / 1000).toString("hh.mm.ss"))
					 .arg((ts / 1000) % 1000, 3, QChar('0'));
		zip.addFile("snapshot_info.txt", infoLines.join("\n").toUtf8());
		return zip.getArchive();
	} else if (filename.endsWith(".bin")) {
		QByteArray all;
		for (int i = snapshots.size() - 1; i >= 0; i--)
			all.append((const char *)snapshots[i].constData(), snapshots[i].size());
		return all;
	}
	if (snapshots.size())
		return QByteArray((const char *)snapshots.first().constData(), snapshots.first().size());
	return QByteArray();
}
