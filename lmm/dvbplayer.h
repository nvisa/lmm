#ifndef DVBPLAYER_H
#define DVBPLAYER_H

#include "baselmmplayer.h"

class V4l2Input;
class FileOutput;

class DvbPlayer : public BaseLmmPlayer
{
	Q_OBJECT
public:
	explicit DvbPlayer(QObject *parent = 0);
	~DvbPlayer();

	int play();
	int stop();
signals:
	
protected slots:
private:
	V4l2Input *v4l2;
};

#endif // DVBPLAYER_H
