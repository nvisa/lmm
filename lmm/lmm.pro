TARGET = lmm
TEMPLATE = lib
CONFIG += staticlib

QT += network

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
    debugserver.cpp \
    debugclient.cpp \
    udpoutput.cpp \
    udpinput.cpp \
    lmmthread.cpp \
    tools/unittimestat.cpp \
    tools/videoutils.cpp \
    tools/systeminfo.cpp \
    rtsp/basertspserver.cpp \
    debug.cpp \
    hardwareoperations.cpp \
    v4l2output.cpp \

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
    debugserver.h \
    debugclient.h \
    udpoutput.h \
    udpinput.h \
    lmmthread.h \
    tools/unittimestat.h \
    tools/videoutils.h \
    tools/systeminfo.h \
    rtsp/basertspserver.h \
    debug.h \
    hardwareoperations.h \
    platform_info.h \
    v4l2output.h \

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
    SOURCES += maddecoder.cpp \
        mp3player.cpp \
        mp3demux.cpp \

    HEADERS += maddecoder.h \
        mp3player.h \
        mp3demux.h \
    LIBS += -lmad -ltag
    DEFINES += CONFIG_MAD
}

ffmpeg {
    HEADERS += \
        ffmpeg/avidecoder.h \
        ffmpeg/avidemux.h \
        ffmpeg/mpegtsdemux.h \
        ffmpeg/baselmmdemux.h \
        ffmpeg/baselmmmux.h \
        ffmpeg/mp4mux.h \
        ffmpeg/avimux.h \
        ffmpeg/dvbplayer.h \
        ffmpeg/rtph264mux.h \
        ffmpeg/rtpmjpegmux.h \
        ffmpeg/rtpmux.h \

    SOURCES += \
        ffmpeg/avidecoder.cpp \
        ffmpeg/baselmmdemux.cpp \
        ffmpeg/mpegtsdemux.cpp \
        ffmpeg/avidemux.cpp \
        ffmpeg/baselmmmux.cpp \
        ffmpeg/mp4mux.cpp \
        ffmpeg/avimux.cpp \
        ffmpeg/dvbplayer.cpp \
        ffmpeg/rtph264mux.cpp \
        ffmpeg/rtpmjpegmux.cpp \
        ffmpeg/rtpmux.cpp \

    DEFINES += CONFIG_FFMPEG
}

dmai {
    SOURCES += dmai/dmaiencoder.cpp \
        dmai/dmaibuffer.cpp \
        dmai/cpuload.cpp \
        dmai/jpegencoder.cpp \
        dmai/h264encoder.cpp \
        dmai/dmaidecoder.cpp \
        dmai/videotestsource.cpp \

    HEADERS += dmai/dmaiencoder.h \
        dmai/dmaibuffer.h \
        dmai/cpuload.h \
        dmai/jpegencoder.h \
        dmai/h264encoder.h \
        dmai/dmaidecoder.h \
        dmai/videotestsource.h \

}

dm365 {
    include(dm365/tipaths.pri)
    DEFINES += CONFIG_DM365
    SOURCES += \
        dm365/dm365camerainput.cpp \
        dm365/dm365videooutput.cpp \
        dm365/platformcommondm365.cpp \
        textoverlay.cpp \


    HEADERS += dm365/dm365camerainput.h \
        dm365/dm365videooutput.h \
        dm365/platformcommondm365.h \
        textoverlay.h \

    xdc.files += dm365/tipaths.pri
    xdc.files += dm365/dm365.pri
    xdc.files += dm365/config.bld
    xdc.files += dm365/dm365.cfg
    xdc.path = /usr/local/include/lmm/dm365
    CONFIG += arm

    OTHER_FILES += \
        dm365/dm365.pri \
        dm365/dm365.cfg \
        dm365/config.bld
}

dvb {
    SOURCES += \
        dvb/tsdemux.cpp \
        dvb/dvbutils.cpp \

    HEADERS  += \
        dvb/tsdemux.h \
        dvb/dvbutils.h \
}

dm6446 {
    include(dm6446/dm6446.pri)
    DEFINES += CONFIG_DM6446
    SOURCES += \
        dm6446/dm6446fboutput.cpp \
        dm6446/platformcommondm6446.cpp \

    HEADERS  += \
        dm6446/blec32fboutput.h \
        dm6446/platformcommondm6446.h \

    xdc.files += dm6446/xdc_linker.cmd
    xdc.path = /usr/local/share/lmm
    CONFIG += arm
}

armv5te {
    CONFIG += arm
}

x86 {
    DEFINES += TARGET_x86
}

arm {
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

#Add make targets for checking version info
VersionCheck.commands = @$$PWD/checkversion.sh $$PWD
QMAKE_EXTRA_TARGETS += VersionCheck
PRE_TARGETDEPS += VersionCheck
