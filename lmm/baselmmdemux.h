#ifndef BASELMMDEMUX_H
#define BASELMMDEMUX_H

#include "baselmmelement.h"

struct AVPacket;
struct AVFormatContext;
struct AVStream;
class RawBuffer;

class BaseLmmDemux : public BaseLmmElement
{
	Q_OBJECT
public:
	enum streamType {
		STREAM_VIDEO,
		STREAM_AUDIO
	};
	explicit BaseLmmDemux(QObject *parent = 0);
	int setSource(QString filename);
	qint64 getTotalDuration();
	qint64 getCurrentPosition();
	RawBuffer * nextAudioBuffer();
	RawBuffer * nextVideoBuffer();
	int audioBufferCount();
	int start();
	int stop();
	int seekTo(qint64 pos);
	int demuxOne();
signals:
	
public slots:
protected:
	qint64 streamPosition;
	int videoStreamIndex;
	int audioStreamIndex;
	AVFormatContext *context;
	AVStream *audioStream;
	AVStream *videoStream;
	QList<RawBuffer *> audioBuffers;
	QList<RawBuffer *> videoBuffers;

	/* derived stats */
	unsigned int audioTimeBase; /* in usecs */
	unsigned int videoTimeBase; /* in usecs */

	AVPacket * nextPacket();
};

#endif // BASELMMDEMUX_H
