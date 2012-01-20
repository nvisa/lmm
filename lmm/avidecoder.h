#ifndef AVIDECODER_H
#define AVIDECODER_H

#include "baselmmplayer.h"

class AviDemux;
class Mad;
class AlsaOutput;
class FbOutput;
class DmaiDecoder;
class QTimer;
class StreamTime;
class BaseLmmElement;

class AviPlayer : public BaseLmmPlayer
{
	Q_OBJECT
public:
	explicit AviPlayer(QObject *parent = 0);
	~AviPlayer();
	int startDecoding();
	void stopDecoding();
private:
};

#endif // AVIDECODER_H
