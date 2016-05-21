#ifndef BASELMMDEMUX_H
#define BASELMMDEMUX_H

#include <lmm/baselmmelement.h>

#include <stdint.h>

struct AVPacket;
struct AVFormatContext;
struct AVStream;
class RawBuffer;
class StreamTime;
struct URLContext;
struct AVCodecContext;

class BaseLmmDemux : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit BaseLmmDemux(QObject *parent = 0);
	virtual int setSource(QString filename);
	virtual int getFrameDuration(int st);
	virtual qint64 getTotalDuration();
	virtual qint64 getCurrentPosition();
	virtual int start();
	virtual int stop();
	virtual int seekTo(qint64 pos);
	virtual int demuxOne();
	virtual int processBuffer(const RawBuffer &buf);
	int getDemuxedCount();
	int processBlocking(int ch = 0);
	AVCodecContext * getVideoCodecContext();
	AVCodecContext * getAudioCodecContext();
	bool isAvformatContextDone() { return unblockContext; }

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
	bool demuxAudio;
	bool demuxVideo;
	bool foundStreamInfo;
	int demuxNumber;
	QMutex conlock;
	int avioBufferSize;
	uchar *avioBuffer;
	QHash<int, int> demuxedCount;
	bool unblockContext;
	/*
	 * In older FFmpeg releases AVIOContext is typedef to
	 * anonymous struct so it is not possible to forward
	 * declare here. This behaviour is fixed in later releases
	 * but we define this as 'void *' to be compatiple with
	 * old releases.
	 */
	void *avioCtx;

	/* derived stats */
	qint64 audioTimeBaseN;		/* in nano secs */
	qint64 videoTimeBaseN;		/* in nano secs */

	virtual AVPacket * nextPacket();
	virtual int findStreamInfo();
	virtual QString mimeType();
};

#endif // BASELMMDEMUX_H
