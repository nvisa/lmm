#include "rtph264mux.h"

#include <emdesk/debug.h>

RtpH264Mux::RtpH264Mux(QObject *parent) :
	RtpMux(parent)
{
}

Lmm::CodecType RtpH264Mux::codecType()
{
	return Lmm::CODEC_H264;
}
