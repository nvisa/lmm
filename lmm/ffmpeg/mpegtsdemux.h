#ifndef MPEGTSDEMUX_H
#define MPEGTSDEMUX_H

#include <lmm/ffmpeg/baselmmdemux.h>

class CircularBuffer;

class MpegTsDemux : public BaseLmmDemux
{
	Q_OBJECT
public:
	explicit MpegTsDemux(QObject *parent = 0);
	virtual int start();
	virtual int stop();
	virtual int seekTo(qint64 pos);
signals:
	
public slots:
private:
};

#endif // MPEGTSDEMUX_H
