#ifndef MP4MUX_H
#define MP4MUX_H

#include <lmm/ffmpeg/baselmmmux.h>

class Mp4Mux : public BaseLmmMux
{
	Q_OBJECT
public:
	explicit Mp4Mux(QObject *parent = 0);
	virtual int64_t seekUrl(int64_t pos, int whence);
signals:
	
public slots:
protected:
	QString mimeType();
};

#endif // MP4MUX_H
