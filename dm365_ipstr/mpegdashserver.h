#ifndef MPEGDASHSERVER_H
#define MPEGDASHSERVER_H

#include <QMutex>
#include <QObject>
#include <QElapsedTimer>

#include <lmm/rawbuffer.h>

class MpegDashServer : public QObject
{
	Q_OBJECT
public:
	explicit MpegDashServer(QObject *parent = 0);
	void addBuffer(RawBuffer buf);

protected:
	const QByteArray makeDashHeader(int width, int height);
	const QByteArray makeDashSegment(int segmentSeqNo, int frameDuration);

	int fragCount;
	QList<QList<RawBuffer> > gops;
	QList<RawBuffer> currentGOP;
	QElapsedTimer dashElapsed;
	QMutex buflock;
};

#endif // MPEGDASHSERVER_H
