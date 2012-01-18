#ifndef AVIDEMUX_H
#define AVIDEMUX_H

#include "baselmmelement.h"

struct AVPacket;
struct AVFormatContext;
struct AVStream;
class RawBuffer;
class BaseLmmElement;

class AviDemux : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit AviDemux(QObject *parent = 0);
	int setSource(QString filename);
	qint64 getTotalDuration();
	qint64 getCurrentPosition();
	RawBuffer * nextAudioBuffer();
	RawBuffer * nextVideoBuffer();
	int audioBufferCount();
	int start();
	int stop();
	int seekTo(qint64 pos);
signals:
	void newAudioFrame();
	void newVideoFrame();
public slots:
	int demuxAll();
	int demuxOne();
private:
	int videoStreamIndex;
	int audioStreamIndex;
	AVFormatContext *context;
	AVStream *audioStream;
	AVStream *videoStream;
	QList<RawBuffer *> audioBuffers;
	QList<RawBuffer *> videoBuffers;

	qint64 streamPosition;

	/* derived stats */
	unsigned int audioTimeBase; /* in usecs */
	unsigned int videoTimeBase; /* in usecs */
	AVPacket * nextPacket();
};

#endif // AVIDEMUX_H
