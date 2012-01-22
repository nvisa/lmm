/*
 * Copyright (C) 2010 Yusuf Caglar Akyuz
 *
 * Her hakki Bilkon Ltd. Sti.'ye  aittir.
 */

#ifndef DVBUTILS_H
#define DVBUTILS_H

#include <QObject>

struct dvb_ch_info {
    QString name;
    int freq;
    int pol;
    int symRate;
    int vpid;
    int apid;
    int spid;
};

class DVBUtils : public QObject
{
    Q_OBJECT
public:    
    enum channel_pol {
        HOR_POL = 1,
        VER_POL = 2,
    };

    explicit DVBUtils(QObject *parent = 0);

    static bool tuneToChannel(QString ch);

    static int checkStatus();
    static struct dvb_ch_info currentChannelInfo();

    static QStringList channelList(QString listName = "");
    static QStringList tvChannelList(QString listName = "");
    static QStringList radioChannelList(QString listName = "");

    static QStringList fromTurksatList(QString packet = "all");
    static void fillDbFromTurksatList();

    static QStringList satteliteLists();
    static void selectSatteliteList(QString listName);

signals:

public slots:
private:
    static int zapTo();
    static int zapTo(int freq, enum channel_pol pol, int symRate);
    static int zapTo(QString channelName);
    static int selectLNB(channel_pol pol, int hiband);

};

#endif // DVBUTILS_H
