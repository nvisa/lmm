#ifndef MP4MUX_H
#define MP4MUX_H

#include <lmm/ffmpeg/baselmmmux.h>

class Mp4Mux : public BaseLmmMux
{
	Q_OBJECT
public:
	explicit Mp4Mux(QObject *parent = 0);
	
signals:
	
public slots:
protected:
	QString mimeType();
};

#endif // MP4MUX_H
