#ifndef MP3DEMUX_H
#define MP3DEMUX_H

#include <lmm/ffmpeg/baselmmdemux.h>

class Mp3Demux : public BaseLmmDemux
{
	Q_OBJECT
public:
	explicit Mp3Demux(QObject *parent = 0);
	QString getArtistName() { return artist; }
	QString getAlbumName() { return album; }
	QString mimeType();
signals:
	
public slots:
private:
	QString artist;
	QString album;

	int findStreamInfo();
};

#endif // MP3DEMUX_H
