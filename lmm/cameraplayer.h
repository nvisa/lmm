#ifndef CAMERAPLAYER_H
#define CAMERAPLAYER_H

#include <lmm/baselmmplayer.h>

class V4l2Input;

class CameraPlayer : public BaseLmmPlayer
{
	Q_OBJECT
public:
	explicit CameraPlayer(QObject *parent = 0);

signals:
	
protected slots:
	virtual int decodeLoop();
protected:
	V4l2Input *input;
};

#endif // CAMERAPLAYER_H
