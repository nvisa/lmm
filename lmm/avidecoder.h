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
class QTime;

class AviDecoder : public QObject
{
	Q_OBJECT
public:
	explicit AviDecoder(QObject *parent = 0);
	~AviDecoder();
	int startDecoding();
	void stopDecoding();
	int getDuration();
	int getPosition();
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
	QTime *streamTime;

	void audioLoop();
	void videoLoop();
};

#endif // AVIDECODER_H
