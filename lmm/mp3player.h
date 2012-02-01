#ifndef MP3PLAYER_H
#define MP3PLAYER_H

#include "baselmmplayer.h"

class Mp3Player : public BaseLmmPlayer
{
	Q_OBJECT
public:
	explicit Mp3Player(QObject *parent = 0);
	
signals:
	
public slots:
	
};

#endif // MP3PLAYER_H
