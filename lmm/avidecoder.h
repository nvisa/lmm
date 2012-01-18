#ifndef AVIDECODER_H
#define AVIDECODER_H

#include <QObject>
#include <QFuture>

class AviDemux;
class Mad;
class AlsaOutput;
class FbOutput;
class DmaiDecoder;
class QTimer;
class StreamTime;
class BaseLmmElement;

class AviDecoder : public QObject
{
	Q_OBJECT
public:
	explicit AviDecoder(QObject *parent = 0);
	~AviDecoder();
	int startDecoding();
	void stopDecoding();
	qint64 getDuration();
	qint64 getPosition();
public slots:
	void newAudioFrame();
	void decodeLoop();
private:
	enum runState {
		RUNNING,
		STOPPED
	};
	runState state;
	AviDemux *demux;
	Mad *audioDecoder;
	DmaiDecoder *videoDecoder;
	AlsaOutput *audioOutput;
	FbOutput *videoOutput;
	QTimer *timer;
	StreamTime *streamTime;
	QList<BaseLmmElement *> elements;

	void audioLoop();
	void videoLoop();
};

#endif // AVIDECODER_H
