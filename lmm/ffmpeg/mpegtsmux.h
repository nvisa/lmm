#ifndef MPEGTSMUX_H
#define MPEGTSMUX_H

#include <lmm/ffmpeg/baselmmmux.h>

class MpegTsMux : public BaseLmmMux
{
	Q_OBJECT
public:
	explicit MpegTsMux(QObject *parent = 0);
	
signals:
	
public slots:
protected:
	QString mimeType();
	int initMuxer();
	int findInputStreamInfo();
	qint64 packetTimestamp(int stream);
	int addAudioStream();
};

#endif // MPEGTSMUX_H
