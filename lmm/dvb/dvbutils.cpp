/*
 * Copyright (C) 2010 Yusuf Caglar Akyuz
 *
 * Her hakki Bilkon Ltd. Sti.'ye  aittir.
 */
#if 0
#include "dvbutils.h"

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/param.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <stdint.h>
#include <sys/time.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/audio.h>
#ifdef TARGET_ARM
#include <linux/i2c-dev.h>
#endif
#include <linux/i2c.h>

#include "emdesk/platform_info.h"

#include "emdesk/debug.h"
#include "lib/qttools.h"
#include "lib/dbtools.h"

#include <QMap>
#include <QSqlDriver>
#include <QSqlTableModel>
#include <QSqlRecord>
#include <QSqlError>

#define FRONTENDDEVICE "/dev/dvb/adapter%d/frontend%d"
#define DEMUXDEVICE "/dev/dvb/adapter%d/demux%d"
#define AUDIODEVICE "/dev/dvb/adapter%d/audio%d"

static QMap<QString, QString> channels;
static QMap<QString, QString> tvChannels;
static QMap<QString, QString> radioChannels;
static QStringList chLists;
static QString currentSelectedList;
static struct dvb_ch_info currentCh;

struct lnb_types_st {
    unsigned long	low_val;
    unsigned long	high_val;	/* zero indicates no hiband */
    unsigned long	switch_val;	/* zero indicates no hiband */
};

static struct lnb_types_st lnb_type = {
    9750000,
    10600000,
    11700000,
};

struct diseqc_cmd {
    struct dvb_diseqc_master_cmd cmd;
    uint32_t wait;
};

static int fefd = 0;

static void diseqc_send_msg(int fd, fe_sec_voltage_t v, struct diseqc_cmd *cmd,
                            fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b)
{
    if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
        perror("FE_SET_TONE failed");
    if (ioctl(fd, FE_SET_VOLTAGE, v) == -1)
        perror("FE_SET_VOLTAGE failed");
    usleep(15 * 1000);
    if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd->cmd) == -1)
        perror("FE_DISEQC_SEND_MASTER_CMD failed");
    usleep(cmd->wait * 1000);
    usleep(15 * 1000);
    if (ioctl(fd, FE_DISEQC_SEND_BURST, b) == -1)
        perror("FE_DISEQC_SEND_BURST failed");
    usleep(15 * 1000);
    if (ioctl(fd, FE_SET_TONE, t) == -1)
        perror("FE_SET_TONE failed");
}




/* digital satellite equipment control,
 * specification is available from http://www.eutelsat.com/
 */
static int diseqc(int secfd, int sat_no, int pol_vert, int hi_band)
{
    struct diseqc_cmd cmd =
    { {{0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4}, 0 };

    /* param: high nibble: reset bits, low nibble set bits,
    * bits are: option, position, polarization, band
    */
    cmd.cmd.msg[3] =
            0xf0 | (((sat_no * 4) & 0x0f) | (hi_band ? 1 : 0) | (pol_vert ? 0 : 2));

    diseqc_send_msg(secfd, pol_vert ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18,
                    &cmd, hi_band ? SEC_TONE_ON : SEC_TONE_OFF,
                    sat_no % 2 ? SEC_MINI_B : SEC_MINI_A);

    return TRUE;
}

static int do_tune(int fefd, unsigned int ifreq, unsigned int sr)
{
    struct dvb_frontend_parameters tuneto;
    struct dvb_frontend_event ev;

    /* discard stale QPSK events */
    while (1) {
        if (ioctl(fefd, FE_GET_EVENT, &ev) == -1)
            break;
    }

    tuneto.frequency = ifreq;
    tuneto.inversion = INVERSION_AUTO;
    tuneto.u.qpsk.symbol_rate = sr;
    tuneto.u.qpsk.fec_inner = FEC_AUTO;

    if (ioctl(fefd, FE_SET_FRONTEND, &tuneto) == -1) {
        perror("FE_SET_FRONTEND failed");
        return FALSE;
    }

    return TRUE;
}


static int check_frontend (int fe_fd, int dvr, int human_readable)
{
    (void)dvr;
    fe_status_t status;
    uint16_t snr, signal;
    uint32_t ber, uncorrected_blocks;
    int timeout = 0;

    do {
        if (ioctl(fe_fd, FE_READ_STATUS, &status) == -1)
            perror("FE_READ_STATUS failed");
        /*
         * some frontends might not support all these ioctls, thus we
         * avoid printing errors
         */
        if (ioctl(fe_fd, FE_READ_SIGNAL_STRENGTH, &signal) == -1)
            signal = -2;
        if (ioctl(fe_fd, FE_READ_SNR, &snr) == -1)
            snr = -2;
        if (ioctl(fe_fd, FE_READ_BER, &ber) == -1)
            ber = -2;
        if (ioctl(fe_fd, FE_READ_UNCORRECTED_BLOCKS, &uncorrected_blocks) == -1)
            uncorrected_blocks = -2;

        if (human_readable) {
            mInfo ("status %02x | signal %3u%% | snr %3u%% | ber %d | unc %d | ",
                   status, (signal * 100) / 0xffff, (snr * 100) / 0xffff, ber, uncorrected_blocks);
        } else {
            mInfo ("status %02x | signal %04x | snr %04x | ber %08x | unc %08x | ",
                   status, signal, snr, ber, uncorrected_blocks);
        }

        if (status & FE_HAS_LOCK) {
            mInfo("FE_HAS_LOCK\n");
        } else {
            mInfo("\n");
        }

        if (((status & FE_HAS_LOCK) || (++timeout >= 10)))
            break;

        usleep(100000);
    } while (1);

    return status;
}

static int get_highband(unsigned int freq)
{
    int hiband = 0;
    if (lnb_type.switch_val && lnb_type.high_val &&
        freq >= lnb_type.switch_val)
        hiband = 1;

    return hiband;
}

static int zap_to(unsigned int adapter, unsigned int frontend,
                   unsigned int sat_no, unsigned int freq, unsigned int pol,
                   unsigned int sr, int dvr, int human_readable)
{
    char fedev[128];
    uint32_t ifreq;
    int hiband, result;
    static struct dvb_frontend_info fe_info;

    if (!fefd) {
        snprintf(fedev, sizeof(fedev), FRONTENDDEVICE, adapter, frontend);
        mInfo("using '%s'", fedev);

        if ((fefd = open(fedev, O_RDWR | O_NONBLOCK)) < 0) {
            //perror("opening frontend failed");
            fefd = 0;
            return -1;
        }

        result = ioctl(fefd, FE_GET_INFO, &fe_info);

        if (result < 0) {
            perror("ioctl FE_GET_INFO failed");
            close(fefd);
            return -1;
        }

        if (fe_info.type != FE_QPSK) {
            fprintf(stderr, "frontend device is not a QPSK (DVB-S) device!\n");
            close(fefd);
            return -1;
        }
    }

    hiband = get_highband(freq);
    if (hiband)
        ifreq = freq - lnb_type.high_val;
    else {
        if (freq < lnb_type.low_val)
            ifreq = lnb_type.low_val - freq;
        else
            ifreq = freq - lnb_type.low_val;
    }

    if (diseqc(fefd, sat_no, pol, hiband))
        do_tune(fefd, ifreq, sr);

    return check_frontend (fefd, dvr, human_readable);
}

DVBUtils::DVBUtils(QObject *parent) :
        QObject(parent)
{
}

int DVBUtils::zapTo()
{
    /*
     * one line of the VDR channel file has the following format:
     * ^name:frequency_MHz:polarization:sat_no:symbolrate:vpid:apid:?:service_id$
     *
     * e.g. : TRT 1:11919:v:0:24444:512:513:1
     */
    return zap_to(0, 0, 0, 11919 * 1000, VER_POL, 24444 * 1000, 0, 0);
}

int DVBUtils::zapTo(int freq, enum channel_pol pol, int symRate)
{
    return (zap_to(0, 0, 0, freq * 1000, pol, symRate * 1000, 0, 0) & FE_HAS_LOCK) ? 0 : 1;
}
#ifdef TARGET_ARM
static inline __s32 i2c_smbus_access(int file, char read_write, __u8 command,
                                     int size, union i2c_smbus_data *data)
{
    struct i2c_smbus_ioctl_data args;

    args.read_write = read_write;
    args.command = command;
    args.size = size;
    args.data = data;
    return ioctl(file,I2C_SMBUS,&args);
}

static inline __s32 i2c_smbus_write_byte_data(int file, __u8 command,
                                              __u8 value)
{
    union i2c_smbus_data data;
    data.byte = value;
    return i2c_smbus_access(file,I2C_SMBUS_WRITE,command,
                            I2C_SMBUS_BYTE_DATA, &data);
}


static int i2cFile = -1;
int DVBUtils::selectLNB(channel_pol pol, int hiband)
{
    if (i2cFile < 0) {
        i2cFile = open("/dev/i2c-1", O_RDWR);

        /* With force, let the user read from/write to the registers
               even when a driver is also running */
        if (ioctl(i2cFile, I2C_SLAVE_FORCE, 0xa) < 0) {
            fprintf(stderr,
                    "Error: Could not set address to 0x%02x: %s\n",
                    0xa, strerror(errno));
            return -errno;
        }
    }

    i2c_smbus_write_byte_data(i2cFile, 0x60, 0x60);
    usleep(10000);
    if (pol == VER_POL) {
        if (hiband)
            i2c_smbus_write_byte_data(i2cFile, 0x64, 0x64);
        else
            i2c_smbus_write_byte_data(i2cFile, 0x04, 0x04);
        usleep(10000);
    } else {
        if (hiband)
            i2c_smbus_write_byte_data(i2cFile, 0x68, 0x68);
        else
            i2c_smbus_write_byte_data(i2cFile, 0x8, 0x8);
        usleep(10000);
        if (hiband)
            i2c_smbus_write_byte_data(i2cFile, 0x6c, 0x6c);
        else
            i2c_smbus_write_byte_data(i2cFile, 0xc, 0xc);
        usleep(10000);
    }

    return 0;
}
#else
int DVBUtils::selectLNB(channel_pol, int)
{
    return -ENODEV;
}
#endif
int DVBUtils::zapTo(QString name)
{
    if (!channels.contains(name))
        return -ENOENT;

    QStringList f = channels[name].split(":");
    if (f.size() < 8)
        return -EINVAL;

    currentCh.name = name;
    currentCh.freq = f[1].toInt();
    channel_pol nPol = f[2] == "v" ? VER_POL : HOR_POL;
    /* polarity should be changed */
    currentCh.pol = nPol;
    DVBUtils::selectLNB(nPol, get_highband(currentCh.freq * 1000));
    mInfo("LNB polarity has changed to %s\n", qPrintable(f[2]));
    currentCh.symRate = f[4].toInt();
    currentCh.vpid = f[5].toInt();
    currentCh.apid = f[6].toInt();
    currentCh.spid = f[7].toInt();

    return (zap_to(0, 0, 0, currentCh.freq * 1000, currentCh.pol, currentCh.symRate * 1000, 0, 0) & FE_HAS_LOCK) > 0 ? 0 : 1;
}

bool DVBUtils::tuneToChannel(QString ch)
{
    QTime t;
    t.start();
    mInfo("tuning to channel %s", qPrintable(ch));
    int err = DVBUtils::zapTo(ch);
    if (err == -ENOENT) {
        mInfo("no such channel");
        return false;
    }
    if (err == -EINVAL) {
        mInfo("wrong channel parameters");
        return false;
    }
    while (DVBUtils::checkStatus()) {
        DVBUtils::zapTo(ch);
        if (t.elapsed() > 5000)
            return false;
    }
    mInfo("tuned channel to %s", qPrintable(ch));

    return true;
}

int DVBUtils::checkStatus()
{
    if (fefd <= 0)
        return -1;

    return (check_frontend(fefd, 0, 0) & FE_HAS_LOCK) ? 0 : 1;
}

QStringList DVBUtils::fromTurksatList(QString packet)
{
    mDebug("creating chlist for Turksat packet %s\n", qPrintable(packet));
    QStringList entries;
    QFile xmlFile("kanallistesi.xml");
    if(!xmlFile.open(QIODevice::ReadOnly))
        return QStringList();

    bool exportPacket = false;
    QString currentPacket;
    QXmlStreamReader xml(&xmlFile);
    while (!xml.atEnd()) {
        QXmlStreamReader::TokenType type = xml.readNext();
        if (type == QXmlStreamReader::StartElement) {
            QXmlStreamAttributes attr = xml.attributes();
            if (xml.name() == "Paket") {
                if (packet != "") {
                    if (packet == "all" || attr.value("adi").toString().startsWith(packet))
                        exportPacket = true;
                    else
                        exportPacket = false;
                } else if (attr.value("adi") == packet)
                    exportPacket = true;
                else
                    exportPacket = false;
                if (exportPacket) {
                    currentPacket = attr.value("adi").toString();
                    if (currentPacket == "")
                        currentPacket = "N/A";
                }
            } else if (xml.name() == "Kanal") {
                bool free = true;
                if (attr.value("Sifreli").toString() == "Sifreli")
                    free =false;
                if (exportPacket && free) {
                    /*
                     * one line of the VDR channel file has the following format:
                     * ^name:frequency_MHz:polarization:sat_no:symbolrate:vpid:apid:?:service_id$
                     *
                     * e.g. : TRT 1:11919:v:0:24444:512:513:1
                     */
                    QString pol;
                    if (attr.value("Polarizasyon").toString() == "Horizontal")
                        pol = "h";
                    else
                        pol = "v";
                    QString vpid;
                    if (attr.value("YayinBicimi").toString() == "TV")
                        vpid = attr.value("VideoPID").toString();
                    else
                        vpid = "0";
                    QString entry = QString("%1:%2:%3:%4:%5:%6:%7:%8:%9")
                            .arg(attr.value("adi").toString())
                            .arg(attr.value("Frekans").toString())
                            .arg(pol)
                            .arg(0)
                            .arg(attr.value("SymbolRate").toString())
                            .arg(vpid)
                            .arg(attr.value("AudioPID").toString())
                            .arg(0)
                            .arg(currentPacket)
                            ;
                    entries << entry;
                }
            }
        }
        if (type == QXmlStreamReader::EndElement) {
        }
    }

    if (xml.hasError())
        mDebug("%s", qPrintable(xml.errorString()));

    return entries;
}

void DVBUtils::fillDbFromTurksatList()
{
    QStringList channels = fromTurksatList("all");

    dbtools::database().driver()->beginTransaction();
    QSqlTableModel model;
    model.setEditStrategy(QSqlTableModel::OnManualSubmit);
    model.setTable("uyduKanallari");
    model.select();
    QSqlRecord rec;
    rec = model.record();
    foreach(QString ch, channels) {
        QSqlRecord rec = model.record();
        QStringList fields = ch.split(":");
        rec.setValue("kanalIsmi", QVariant(fields[0]));
        rec.setValue("frekans", QVariant(fields[1]));
        rec.setValue("polarizasyon", QVariant(fields[2]));
        rec.setValue("sembolHizi", QVariant(fields[4]));
        rec.setValue("videoPid", QVariant(fields[5]));
        rec.setValue("audioPid", QVariant(fields[6]));
        rec.setValue("paket", QVariant(fields[8]));
        model.insertRecord(-1, rec);
    }
    if(!model.submitAll())
        mDebug("submit error: %s", qPrintable(model.lastError().text()));
    dbtools::database().driver()->commitTransaction();
}

QStringList DVBUtils::satteliteLists()
{
    if (chLists.size() == 0) {
        QSqlQueryModel model;
        model.setQuery("SELECT DISTINCT paket FROM uyduKanallari");
        for (int i = 0; i < model.rowCount(); i++)
            chLists << model.data(model.index(i, 0)).toString();
    }
    return chLists;
}

void DVBUtils::selectSatteliteList(QString listName)
{
    if (chLists.size() == 0)
        satteliteLists();

    if (listName == currentSelectedList)
        return;

    currentSelectedList = listName;
    channels.clear();
    tvChannels.clear();
    radioChannels.clear();

    if (chLists.contains(listName) || listName == "all") {
        QSqlQueryModel model;
        if (listName != "all")
            model.setQuery(QString("SELECT * FROM uyduKanallari WHERE paket='%1'")
                       .arg(listName));
        else
            model.setQuery(QString("SELECT * FROM uyduKanallari"));
        for (int i = 0; i < model.rowCount(); i++) {
            QSqlRecord rec = model.record(i);
            QString ch = QString("%1:%2:%3:%4:%5:%6:%7:%8")
                    .arg(rec.value("kanalIsmi").toString())
                    .arg(rec.value("frekans").toString())
                    .arg(rec.value("polarizasyon").toString())
                    .arg(0)
                    .arg(rec.value("sembolHizi").toString())
                    .arg(rec.value("videoPid").toString())
                    .arg(rec.value("audioPid").toString())
                    .arg(0)
                    ;
            channels.insert(rec.value("kanalIsmi").toString().trimmed(), ch);
            if (rec.value("videoPid").toInt() != 0)
                tvChannels.insert(rec.value("kanalIsmi").toString().trimmed(), ch);
            else
                radioChannels.insert(rec.value("kanalIsmi").toString().trimmed(), ch);
        }
    }
}

QStringList DVBUtils::channelList(QString listName)
{
    /* first check database */
    if (listName == "")
        listName = currentSelectedList;

    selectSatteliteList(listName);

    /* if database contains nothing about channels, then use szap file */
    if (channels.size() == 0) {
        /* We need to create channel info */
        QStringList lines;
        if (cpu_is_arm())
            lines = DataDiskIO::ImportTextData("/home/root/.szap/channels.conf").split("\n");
        else if (cpu_is_x86())
            lines = DataDiskIO::ImportTextData("channels.conf").split("\n");
        QString line;
        QStringList fields;
        foreach(line, lines) {
            fields = line.split(":");
            if (fields.size() < 8)
                continue;
            channels.insert(fields[0], line);
            if (fields[5].toInt() != 0)
                tvChannels.insert(fields[0].trimmed(), line);
            else
                radioChannels.insert(fields[0].trimmed(), line);
        }
    }
    return channels.keys();
}

QStringList DVBUtils::tvChannelList(QString listName)
{
    channelList(listName);
    return tvChannels.keys();
}

QStringList DVBUtils::radioChannelList(QString listName)
{
    channelList(listName);
    return radioChannels.keys();
}

struct dvb_ch_info DVBUtils::currentChannelInfo()
{
    return currentCh;
}
#endif
