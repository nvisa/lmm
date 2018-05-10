#ifndef VIDEOSTREAMINTERFACE_H
#define VIDEOSTREAMINTERFACE_H

#include <lmm/rawbuffer.h>

class VideoStreamInterface
{
public:
	VideoStreamInterface();

	virtual int processFrameBuffer(const RawBuffer &buf) = 0;
};

#endif // VIDEOSTREAMINTERFACE_H
