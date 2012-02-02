#ifndef BASELMMPLAYER_H
#define BASELMMPLAYER_H

#include <QObject>
#include <QList>
#include <QFuture>

class BaseLmmElement;
class BaseLmmDemux;
class BaseLmmDecoder;
class BaseLmmOutput;
class StreamTime;
class QTimer;
class Alsa;

class BaseLmmPlayer : public QObject
{
	Q_OBJECT
public:
	enum runState {
		RUNNING,
		STOPPED
	};
	explicit BaseLmmPlayer(QObject *parent = 0);
	~BaseLmmPlayer();
	virtual int play(QString url);
	virtual int stop();
	virtual int pause();
	virtual int resume();
	virtual runState getRunningState() { return state; }
	virtual qint64 getDuration();
	virtual qint64 getPosition();
	virtual int seekTo(qint64 pos);
	virtual int seek(qint64 value);
	virtual int flush();

	const QList<BaseLmmElement *> getElements() { return elements; }

	void setMute(bool mute);
	void setVolumeLevel(int per);
	int getVolumeLevel();
signals:
	
protected slots:
	virtual int decodeLoop();
	void audioPopTimerTimeout();
	void streamInfoFound();
protected:
	runState state;
	QList<BaseLmmElement *> elements;
	StreamTime *streamTime;
	BaseLmmElement *mainSource;
	BaseLmmDemux *demux;
	BaseLmmDecoder *audioDecoder;
	BaseLmmDecoder *videoDecoder;
	BaseLmmOutput *audioOutput;
	BaseLmmOutput *videoOutput;
	QTimer *timer;
#ifdef CONFIG_ALSA
	Alsa *alsaControl;
#endif
	bool live;
private:
	void audioLoop();
	void videoLoop();

	QFuture<int> videoDecodeFuture;
};

#endif // BASELMMPLAYER_H
