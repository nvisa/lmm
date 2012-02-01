#ifndef MP3DEMUX_H
#define MP3DEMUX_H

#include "baselmmdemux.h"

class Mp3Demux : public BaseLmmDemux
{
	Q_OBJECT
public:
	explicit Mp3Demux(QObject *parent = 0);
	
signals:
	
public slots:
	
};

#endif // MP3DEMUX_H
