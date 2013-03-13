#ifndef DVBPLAYER_H
#define DVBPLAYER_H

#include <lmm/baselmmplayer.h>

class V4l2Input;
class FileOutput;

class DvbPlayer : public BaseLmmPlayer
{
	Q_OBJECT
public:
	explicit DvbPlayer(QObject *parent = 0);
	int tuneToChannel(QString channelUrl);
	~DvbPlayer();
signals:
	
protected slots:
	int decodeLoop();
private:
};

#endif // DVBPLAYER_H
