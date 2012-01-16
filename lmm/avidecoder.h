#ifndef AVIDECODER_H
#define AVIDECODER_H

#include <QObject>
#include <QFuture>

class AviDemux;
class Mad;
class AlsaOutput;
class QTimer;
class QTime;

class AviDecoder : public QObject
{
	Q_OBJECT
public:
	explicit AviDecoder(QObject *parent = 0);
	int startDecoding();
	int getDuration();
	int getPosition();
public slots:
	void newAudioFrame();
	void decodeLoop();
private:
	AviDemux *demux;
	Mad *audioDecoder;
	AlsaOutput *audioOutput;
	QTimer *timer;
	QTime *streamTime;
	QFuture<void> future;

	void audioLoop();
};

#endif // AVIDECODER_H
