#ifndef AVIMUX_H
#define AVIMUX_H

#include <lmm/ffmpeg/baselmmmux.h>

class AviMux : public BaseLmmMux
{
	Q_OBJECT
public:
	explicit AviMux(QObject *parent = 0);
	
signals:
	
public slots:
	
};

#endif // AVIMUX_H
