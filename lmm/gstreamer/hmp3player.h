#ifndef HMP3PLAYER_H
#define HMP3PLAYER_H

#include <qobject.h>
#include "abstractgstreamerinterface.h"
#include <gst/interfaces/xoverlay.h>
#include <QStringList>
#include <qstring.h>
#include <qmap.h>

class hMp3Player : public AbstractGstreamerInterface
{
public:
    hMp3Player(int winId, QObject *parent = 0);
    hMp3Player(QObject *parent = 0);
    void setMediaSource(QString source) { mp3FileName = source; }
    QString getMediaSource() { return mp3FileName; }
    void setMediaTime(int time) { currTime = time; }

protected:
    int createElementsList();
    void pipelineCreated();
private:
    QString mp3FileName;
    int widgetWinId;
    int currTime;
    QStringList videoFiles;
    QMap<void *, QString> map;
};

#endif // HMP3PLAYER_H
