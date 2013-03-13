#ifndef AVIDEMUX_H
#define AVIDEMUX_H

#include <lmm/ffmpeg/baselmmdemux.h>

struct AVPacket;
struct AVFormatContext;
struct AVStream;
class RawBuffer;

class AviDemux : public BaseLmmDemux
{
	Q_OBJECT
public:
	explicit AviDemux(QObject *parent = 0);

signals:
	void newAudioFrame();
	void newVideoFrame();
public slots:
	int demuxAll();
private:
};

#endif // AVIDEMUX_H
