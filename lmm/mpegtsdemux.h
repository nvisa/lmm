#ifndef MPEGTSDEMUX_H
#define MPEGTSDEMUX_H

#include "baselmmdemux.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
}

class CircularBuffer;

class MpegTsDemux : public BaseLmmDemux
{
	Q_OBJECT
public:
	explicit MpegTsDemux(QObject *parent = 0);
	virtual int setSource(QString) { return -EINVAL; }
	virtual int setSource(CircularBuffer *buf);
	virtual int start();
	virtual int stop();
	virtual int seekTo(qint64 pos);
	virtual int demuxOne();

	int readPacket(uint8_t *buf, int buf_size);
signals:
	
public slots:
private:
	int ffFileBufSize;
	unsigned char *ffFileBuffer;
	ByteIOContext *ffIoContext;
	CircularBuffer *circBuf;
	AVInputFormat *mpegtsraw;

	int startDemuxer();
};

#endif // MPEGTSDEMUX_H
