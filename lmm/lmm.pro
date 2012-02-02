TARGET = lmm
TEMPLATE = lib
CONFIG += staticlib

QT += sql
CONFIG += ffmpeg mad alsa dm6446

SOURCES += \
    filesource.cpp \
    rawbuffer.cpp \
    dmaidecoder.cpp \
    circularbuffer.cpp \
    fboutput.cpp \
    baselmmelement.cpp \
    baselmmdecoder.cpp \
    baselmmoutput.cpp \
    streamtime.cpp \
    baselmmplayer.cpp \
    v4l2input.cpp \
    fileoutput.cpp \
    dvb/tsdemux.cpp \
    dvb/dvbutils.cpp \
    lmmcommon.cpp


HEADERS  += \
    filesource.h \
    rawbuffer.h \
    dmaidecoder.h \
    circularbuffer.h \
    fboutput.h \
    baselmmelement.h \
    baselmmdecoder.h \
    baselmmoutput.h \
    streamtime.h \
    baselmmplayer.h \
    v4l2input.h \
    fileoutput.h \
    dvb/tsdemux.h \
    dvb/dvbutils.h \
    lmmcommon.h

alsa {
    HEADERS += \
        alsa/alsa.h \
        alsaoutput.h
    SOURCES += \
        alsa/alsa.cpp \
        alsaoutput.cpp
    LIBS += -lasound
    DEFINES += CONFIG_ALSA
}

mad {
    SOURCES += maddecoder.cpp
    HEADERS += maddecoder.h
    LIBS += -lmad -ltag
    DEFINES += CONFIG_MAD
}

ffmpeg {
    HEADERS += \
        avidecoder.h \
        avidemux.h \
        mpegtsdemux.h \
        baselmmdemux.h \
        dvbplayer.h \
        mp3player.h \
        mp3demux.h \

    SOURCES += \
        avidecoder.cpp \
        baselmmdemux.cpp \
        mpegtsdemux.cpp \
        avidemux.cpp \
        dvbplayer.cpp \
        mp3player.cpp \
        mp3demux.cpp \

    LIBS += -lavformat
    DEFINES += CONFIG_FFMPEG
}

dm365 {
    include(../dm365.pri)
    SOURCES += \
        lmm/dmaiencoder.cpp \
        lmm/dm365capture.cpp \

    HEADERS += lmm/dmaiencoder.h \
        lmm/dm365capture.h \

}

dm6446 {
    include(../dm6446.pri)
}

CROSS_COMPILE=$$(OE_QMAKE_CC)
isEmpty(CROSS_COMPILE) {
    CONFIG += x86
} else {
    CONFIG += arm
}

x86 {
    include(/home/caglar/myfs/work/tasks/source-codes/bilkon/build/usr/local/include/qtCommon.pri)
    include(/home/caglar/myfs/work/tasks/source-codes/bilkon/build/usr/local/include/emdesk/emdeskCommon.pri)
}
arm {
    include(/home/caglar/myfs/work/tasks/source-codes/bilkon/build/usr/local/include/qtCommon-arm.pri)
    include(/home/caglar/myfs/work/tasks/source-codes/bilkon/build/usr/local/include/emdesk/emdeskCommon-arm.pri)
}

arm {
    TARGET = lmm-arm
}

headers.files = $$HEADERS lmm-arm.pri
headers.path = /usr/local/include/lmm

xdc.files = ../xdc_linker.cmd
xdc.path = /usr/local/share/lmm

target.path = /usr/local/lib

INSTALLS += target headers xdc

OTHER_FILES +=
