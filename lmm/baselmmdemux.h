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

	void setAudioDemuxing(bool v) { demuxAudio = v; } /* TODO: clear existing buffers */
	void setVideoDemuxing(bool v) { demuxVideo = v; } /* TODO: clear existing buffers */

	/* stream information APIs */
	int getAudioSampleRate();
signals:
	void streamInfoFound();
public slots:
protected:
	int libavAnalayzeDuration;
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

	/* derived stats */
	qint64 audioTimeBaseN;		/* in nano secs */
	qint64 videoTimeBaseN;		/* in nano secs */

	virtual AVPacket * nextPacket();
	virtual int findStreamInfo();
};

#endif // BASELMMDEMUX_H
