#-------------------------------------------------
#
# Project created by QtCreator 2012-01-13T13:52:36
#
#-------------------------------------------------

QT       += core gui

TARGET = lmm-demo
TEMPLATE = app

CONFIG += ffmpeg mad alsa dm365

SOURCES += \
    lmm/filesource.cpp \
    lmm/rawbuffer.cpp \
    lmm/dmaidecoder.cpp \
    lmm/circularbuffer.cpp \
    lmm/fboutput.cpp \
    lmm/baselmmelement.cpp \
    lmm/baselmmdecoder.cpp \
    lmm/baselmmoutput.cpp \
    lmm/streamtime.cpp \
    lmm/baselmmplayer.cpp \
    lmm/v4l2input.cpp \
    lmm/fileoutput.cpp \
    lmm/dvb/tsdemux.cpp \
    lmm/dvb/dvbutils.cpp \
    lmm/dmaiencoder.cpp \
    lmm/dm365capture.cpp

HEADERS  += \
    lmm/filesource.h \
    lmm/rawbuffer.h \
    lmm/dmaidecoder.h \
    lmm/circularbuffer.h \
    lmm/fboutput.h \
    lmm/baselmmelement.h \
    lmm/baselmmdecoder.h \
    lmm/baselmmoutput.h \
    lmm/streamtime.h \
    lmm/baselmmplayer.h \
    lmm/v4l2input.h \
    lmm/fileoutput.h \
    lmm/dvb/tsdemux.h \
    lmm/dvb/dvbutils.h \
    lmm/dmaiencoder.h \
    lmm/dm365capture.h

alsa {
    HEADERS += \
        lmm/alsa/alsa.h \
        lmm/alsaoutput.h
    SOURCES += \
        lmm/alsa/alsa.cpp \
        lmm/alsaoutput.cpp
    LIBS += -lasound
    DEFINES += CONFIG_ALSA
}

mad {
    SOURCES += lmm/mad.cpp
    HEADERS += lmm/mad.h
    LIBS += -lmad -ltag
    DEFINES += CONFIG_MAD
}

ffmpeg {
    HEADERS += \
        lmm/avidecoder.h \
        lmm/avidemux.h \
        lmm/mpegtsdemux.h \
        lmm/baselmmdemux.h \
        lmm/dvbplayer.h \

    SOURCES += \
        lmm/avidecoder.cpp \
        lmm/baselmmdemux.cpp \
        lmm/mpegtsdemux.cpp \
        lmm/avidemux.cpp \
        lmm/dvbplayer.cpp \

    LIBS += -lavformat
    DEFINES += CONFIG_FFMPEG
}

FORMS    += lmsdemo.ui

target.path = /usr/local/bin
INSTALLS += target

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

dm365 {
    include(dm365.pri)
    QT -= gui
    SOURCES += main-dm365.cpp

}
dm6446 {
    include(dm6446.pri)
    SOURCES += \
        main.cpp\
        lmsdemo.cpp

    HEADERS += lmsdemo.h
}

QMAKE_CXXFLAGS += -D__STDC_CONSTANT_MACROS

RESOURCES += \
    art.qrc

OTHER_FILES += \
    xdc_linker.cmd \
    dm6446.pri \
    dm365.pri \
    dm365.cfg \
    config.bld
