#ifndef BASELMMDEMUX_H
#define BASELMMDEMUX_H

#include <lmm/baselmmelement.h>

#include <stdint.h>

struct AVPacket;
struct AVFormatContext;
struct AVStream;
class RawBuffer;
class CircularBuffer;
class StreamTime;
struct URLContext;

class BaseLmmDemux : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmDemux(QObject *parent = 0);
	virtual int setSource(QString filename);
	virtual qint64 getTotalDuration();
	virtual qint64 getCurrentPosition();
	virtual RawBuffer nextAudioBuffer();
	virtual RawBuffer nextAudioBufferBlocking();
	virtual RawBuffer nextVideoBuffer();
	virtual RawBuffer nextVideoBufferBlocking();
	virtual int audioBufferCount();
	virtual int videoBufferCount();
	virtual int start();
	virtual int stop();
	virtual int seekTo(qint64 pos);
	virtual int demuxOne();
	virtual int demuxOneBlocking();
	virtual int flush();

	void setAudioDemuxing(bool v) { demuxAudio = v; } /* TODO: clear existing buffers */
	void setVideoDemuxing(bool v) { demuxVideo = v; } /* TODO: clear existing buffers */

	/* stream information APIs */
	int getAudioSampleRate();

	/* ffmpeg url routines */
	int readPacket(uint8_t *buffer, int buf_size);
	int writePacket(const uint8_t *buffer, int buf_size);
	int openUrl(QString url, int flags);
	int closeUrl(URLContext *h);
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
	QList<RawBuffer> audioBuffers;
	QList<RawBuffer> videoBuffers;
	bool demuxAudio;
	bool demuxVideo;
	bool foundStreamInfo;
	int demuxNumber;
	QMutex conlock;

	/* derived stats */
	qint64 audioTimeBaseN;		/* in nano secs */
	qint64 videoTimeBaseN;		/* in nano secs */

	virtual AVPacket * nextPacket();
	virtual int findStreamInfo();
	virtual QString mimeType() = 0;
};

#endif // BASELMMDEMUX_H
