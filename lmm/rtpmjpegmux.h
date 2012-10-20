#ifndef RTPMJPEGMUX_H
#define RTPMJPEGMUX_H

#include <lmm/rtpmux.h>

class RtpMjpegMux : public RtpMux
{
	Q_OBJECT
public:
	explicit RtpMjpegMux(QObject *parent = 0);
	Lmm::CodecType codecType();
signals:
	
public slots:
	
};

#endif // RTPMJPEGMUX_H
