#ifndef RTPH264MUX_H
#define RTPH264MUX_H

#include <lmm/rtpmux.h>

class RtpH264Mux : public RtpMux
{
	Q_OBJECT
public:
	explicit RtpH264Mux(QObject *parent = 0);
	Lmm::CodecType codecType();
protected:
	int initMuxer();
	int findInputStreamInfo();
};

#endif // RTPH264MUX_H
