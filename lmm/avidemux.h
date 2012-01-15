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
	unsigned int getTotalDuration();
	int getCurrentPosition();
	RawBuffer * nextAudioBuffer();
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
	unsigned int audioFrameDuration; /* in usecs */
	AVPacket * nextPacket();
};

#endif // AVIDEMUX_H
