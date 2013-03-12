#ifndef AVIDECODER_H
#define AVIDECODER_H

#include "baselmmplayer.h"

class AviPlayer : public BaseLmmPlayer
{
	Q_OBJECT
public:
	explicit AviPlayer(QObject *parent = 0);
	~AviPlayer();
protected slots:
	virtual int decodeLoop();
private:
};

#endif // AVIDECODER_H
