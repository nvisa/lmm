#ifndef BASELMMDEMUX_H
#define BASELMMDEMUX_H

#include "baselmmelement.h"

struct AVPacket;
struct AVFormatContext;
struct AVStream;
class RawBuffer;
class CircularBuffer;
class StreamTime;

class BaseLmmDemux : public BaseLmmElement
{
	Q_OBJECT
public:
	enum streamType {
		STREAM_VIDEO,
		STREAM_AUDIO
	};
	explicit BaseLmmDemux(QObject *parent = 0);
	virtual int setSource(QString filename);
	virtual qint64 getTotalDuration();
	virtual qint64 getCurrentPosition();
	virtual RawBuffer * nextAudioBuffer();
	virtual RawBuffer * nextVideoBuffer();
	virtual int audioBufferCount();
	virtual int videoBufferCount();
	virtual int start();
	virtual int stop();
	virtual int seekTo(qint64 pos);
	virtual int demuxOne();
	virtual int flush();
	virtual StreamTime * getStreamTime(streamType);

	void setAudioDemuxing(bool v) { demuxAudio = v; }
	void setVideoDemuxing(bool v) { demuxVideo = v; }
signals:
	
public slots:
protected:
	QString sourceUrlName;
	qint64 streamPosition;
	int videoStreamIndex;
	int audioStreamIndex;
	AVFormatContext *context;
	AVStream *audioStream;
	AVStream *videoStream;
	QList<RawBuffer *> audioBuffers;
	QList<RawBuffer *> videoBuffers;
	bool demuxAudio;
	bool demuxVideo;
	bool foundStreamInfo;

	StreamTime *audioClock;
	StreamTime *videoClock;

	/* derived stats */
	unsigned int audioTimeBase; /* in usecs */
	unsigned int videoTimeBase; /* in usecs */
	qint64 audioTimeBaseN;		/* in nano secs */
	qint64 videoTimeBaseN;		/* in nano secs */

	virtual AVPacket * nextPacket();
	int findStreamInfo();
};

#endif // BASELMMDEMUX_H
