TARGET = lmm
TEMPLATE = lib
CONFIG += staticlib

QT += sql network

include (build_config.pri)

SOURCES += \
    filesource.cpp \
    rawbuffer.cpp \
    circularbuffer.cpp \
    baselmmelement.cpp \
    baselmmdecoder.cpp \
    baselmmoutput.cpp \
    streamtime.cpp \
    baselmmplayer.cpp \
    fileoutput.cpp \
    lmmcommon.cpp \
    cameraplayer.cpp \
    v4l2input.cpp \
    fboutput.cpp \
    rtspoutput.cpp \
    debugserver.cpp \
    debugclient.cpp \
    udpoutput.cpp \
    udpinput.cpp \
    lmmthread.cpp \
    videotestsource.cpp \
    dmai/dmaibuffer.cpp

HEADERS  += \
    filesource.h \
    rawbuffer.h \
    circularbuffer.h \
    baselmmelement.h \
    baselmmdecoder.h \
    baselmmoutput.h \
    streamtime.h \
    baselmmplayer.h \
    fileoutput.h \
    lmmcommon.h \
    cameraplayer.h \
    v4l2input.h \
    fboutput.h \
    textoverlay.h \
    rtspoutput.h \
    debugserver.h \
    debugclient.h \
    udpoutput.h \
    udpinput.h \
    lmmthread.h \
    videotestsource.h \
    dmai/dmaibuffer.h

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

gstreamer {
    SOURCES += gstreamer/abstractgstreamerinterface.cpp \
        gstreamer/haviplayer.cpp \
        gstreamer/hmp3player.cpp \

    HEADERS += gstreamer/abstractgstreamerinterface.h \
        gstreamer/haviplayer.h \
        gstreamer/hmp3player.h \

    DEFINES += USE_GSTREAMER
    GST_CFLAGS = $$system(pkg-config gstreamer-0.10 --cflags-only-I | sed 's/-I//g')
    GST_LIBS = -lgstreamer-0.10 -lgobject-2.0 -lgmodule-2.0 -lxml2 -lgthread-2.0 -lrt -lglib-2.0 -lgstinterfaces-0.10
    INCLUDEPATH += $$GST_CFLAGS
    LIBS += $$GST_LIBS -lgstapp-0.10
}

dm365 {
    include(dm365/tipaths.pri)
    DEFINES += CONFIG_DM365
    SOURCES += \
        dmaiencoder.cpp \
        dm365dmaicapture.cpp \
        dm365camerainput.cpp \
        camerastreamer.cpp \
        v4l2output.cpp \
        dm365videooutput.cpp \
        textoverlay.cpp \
        cpuload.cpp \
        jpegencoder.cpp \
        h264encoder.cpp \

    HEADERS += dmaiencoder.h \
        dm365dmaicapture.h \
        camerastreamer.h \
        dm365camerainput.h \
        v4l2output.h \
        dm365videooutput.h \
        textoverlay.h \
        cpuload.h \
        jpegencoder.h \
        h264encoder.h \

    xdc.files += dm365/tipaths.pri
    xdc.files += dm365/dm365.pri
    xdc.files += dm365/config.bld
    xdc.files += dm365/dm365.cfg
    #xdc.files += davinci-common/xdc_linker.cmd
    xdc.path = /usr/local/include/lmm/dm365
    CONFIG += arm

    OTHER_FILES += \
        davinci-common/xdc_linker.cmd \
        dm365/dm365.pri \
        dm365/dm365.cfg \
        dm365/config.bld
}

dm6446 {
    include(../dm6446.pri)
    DEFINES += CONFIG_DM6446
    SOURCES += \
        dmaidecoder.cpp \
        dvb/tsdemux.cpp \
        dvb/dvbutils.cpp \
        blec32tunerinput.cpp \
        blec32fboutput.cpp \
        v4l2output.cpp

    HEADERS  += \
        dmaidecoder.h \
        dvb/tsdemux.h \
        dvb/dvbutils.h \
        blec32tunerinput.h \
        blec32fboutput.h \
        v4l2output.h

    xdc.files += davinci-common/xdc_linker.cmd
    xdc.path = /usr/local/share/lmm
    CONFIG += arm
}

x86 {
    include($$INSTALL_PREFIX/usr/local/include/qtCommon.pri)
    include($$INSTALL_PREFIX/usr/local/include/emdesk/emdeskCommon.pri)
}
arm {
    include($$INSTALL_PREFIX/usr/local/include/qtCommon.pri)
    include($$INSTALL_PREFIX/usr/local/include/emdesk/emdeskCommon.pri)
    DEFINES += TARGET_ARM
}

INCLUDEPATH += ..

headers.files = $$HEADERS lmm.pri
headers.path = /usr/local/include/lmm

target.path = /usr/local/lib

INSTALLS += target headers xdc

OTHER_FILES += \
    build_config.pri \
    lmm.pri \
    dm365/tipaths.pri
