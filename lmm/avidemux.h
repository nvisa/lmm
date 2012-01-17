#ifndef AVIDEMUX_H
#define AVIDEMUX_H

#include <QObject>

struct AVPacket;
struct AVFormatContext;
struct AVStream;
class RawBuffer;

class AviDemux : public QObject
{
	Q_OBJECT
public:
	explicit AviDemux(QObject *parent = 0);
	int setSource(QString filename);
	qint64 getTotalDuration();
	int getCurrentPosition();
	RawBuffer * nextAudioBuffer();
	RawBuffer * nextVideoBuffer();
	int audioBufferCount();
signals:
	void newAudioFrame();
	void newVideoFrame();
public slots:
	void demuxAll();
	void demuxOne();
private:
	int videoStreamIndex;
	int audioStreamIndex;
	AVFormatContext *context;
	AVStream *audioStream;
	AVStream *videoStream;
	QList<RawBuffer *> audioBuffers;
	QList<RawBuffer *> videoBuffers;

	int streamPosition;

	/* derived stats */
	unsigned int audioTimeBase; /* in usecs */
	unsigned int videoTimeBase; /* in usecs */
	AVPacket * nextPacket();
};

#endif // AVIDEMUX_H
