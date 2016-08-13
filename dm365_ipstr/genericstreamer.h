#ifndef GENERICSTREAMER_H
#define GENERICSTREAMER_H

#include <lmm/dm365/ipcamerastreamer.h>

class GenericStreamer : public IPCameraStreamer
{
	Q_OBJECT
public:
	explicit GenericStreamer(QObject *parent = 0);

signals:

public slots:

};

#endif // GENERICSTREAMER_H
