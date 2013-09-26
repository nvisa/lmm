#ifndef SIMPLEVIDEOPLAYER_H
#define SIMPLEVIDEOPLAYER_H

#include <lmm/baseplayer.h>

class BaseLmmDemux;
class BaseLmmOutput;
class FFmpegDecoder;

class SimpleVideoPlayer : public BasePlayer
{
	Q_OBJECT
public:
	explicit SimpleVideoPlayer(QObject *parent = 0);
	bool isEOF() { return eof; }
	virtual qint64 getDuration();
	virtual qint64 getPosition();
	virtual QList<QVariant> extraDebugInfo();
signals:
	
public slots:
protected:
	virtual void timeoutHandler();
	virtual int startPlayer();
	virtual int stopPlayer();
	const QList<LmmThread *> getThreads() { return threads.values(); }
	/* thread functions */
	virtual int demux();
	virtual int queueForVideoDecode();
	virtual int decodeVideo();
	virtual int queueForVideoDisplay();
	virtual int display();

	/* abstract members */
	virtual BaseLmmElement * createVideoOutput() = 0;

	QMap<QString, LmmThread *> threads;

	BaseLmmDemux *demuxer;
	FFmpegDecoder *decoder;
	BaseLmmElement *vout;
	bool waitingEOF;
	bool eof;
	bool waitingDemux;
	int64_t lastPts;
};

#endif // SIMPLEVIDEOPLAYER_H