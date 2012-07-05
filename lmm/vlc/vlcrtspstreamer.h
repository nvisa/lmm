#ifndef VLCRTSPSTREAMER_H
#define VLCRTSPSTREAMER_H

#include "baselmmelement.h"

#include <stdint.h>

class VlcRtspStreamer : public BaseLmmElement
{
	Q_OBJECT
public:
	explicit VlcRtspStreamer(QObject *parent = 0);
	int start();
	int stop();

	/* vlc callbacks TODO: make private and decleare callbacks friend*/
	int vlc_get(int64_t *dts, int64_t *pts, unsigned *flags, size_t * bufferSize, void ** buffer);
	int vlc_release(size_t bufferSize, void * buffer);
signals:
	
public slots:
private:
	int frameCount;
	int fps;
	quint64 base;
	
};

#endif // VLCRTSPSTREAMER_H
