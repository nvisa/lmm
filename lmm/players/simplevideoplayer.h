#ifndef SIMPLEVIDEOPLAYER_H
#define SIMPLEVIDEOPLAYER_H

#include <lmm/baseplayer.h>

#include <QSemaphore>

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
	void setSingleStep(bool v) { singleStep = v; }
	int nextStep();
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
	void checkEOF();
	void threadFinished(LmmThread *thr);

	/* abstract members */
	virtual BaseLmmOutput * createVideoOutput() = 0;

	QMutex thLock;
	QMap<QString, LmmThread *> threads;
	int finishedThreadCount;

	BaseLmmDemux *demuxer;
	FFmpegDecoder *decoder;
	BaseLmmOutput *vout;
	bool waitingEOF;
	bool eof;
	bool waitingDemux;
	int64_t lastPts;
	QSemaphore singleStepWaiter;
	bool singleStep;
};

#endif // SIMPLEVIDEOPLAYER_H
