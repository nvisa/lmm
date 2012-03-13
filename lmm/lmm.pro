TARGET = lmm
TEMPLATE = lib
CONFIG += staticlib

QT += sql
CONFIG += ffmpeg mad alsa dm6446

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
    v4l2output.cpp

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
    v4l2output.h

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
    #include(../dm365.pri)
    QMAKE_CXXFLAGS += -march=armv5t -I"/home/caglar/myfs/work/tasks/aselsan/dm365/ti-dvsdk_dm365-evm_4_02_00_06/dmai_2_20_00_15/packages" -I"/home/caglar/myfs/work/tasks/aselsan/dm365/ti-dvsdk_dm365-evm_4_02_00_06/codec-engine_2_26_02_11/packages" -I"/home/caglar/myfs/work/tasks/aselsan/dm365/ti-dvsdk_dm365-evm_4_02_00_06/framework-components_2_26_00_01/packages" -I"/packages" -I"/home/caglar/myfs/work/tasks/aselsan/dm365/ti-dvsdk_dm365-evm_4_02_00_06/xdais_6_26_01_03/packages" -I"/home/caglar/myfs/work/tasks/aselsan/dm365/ti-dvsdk_dm365-evm_4_02_00_06/linuxutils_2_26_01_02/packages" -I"/home/caglar/myfs/work/tasks/aselsan/dm365/ti-dvsdk_dm365-evm_4_02_00_06/codecs-dm365_4_02_00_00/packages" -I"/home/caglar/myfs/work/tasks/aselsan/dm365/ti-dvsdk_dm365-evm_4_02_00_06/codec-engine_2_26_02_11/examples" -I"/home/caglar/myfs/work/tasks/aselsan/dm365/ti-dvsdk_dm365-evm_4_02_00_06/xdctools_3_16_03_36/packages" -I"/home/caglar/myfs/work/tasks/source-codes/bilkon/lmm-demo-build-desktop-Qt_4_7_1__arm__Release/dm365_config/.."  -Dxdc_target_types__="gnu/targets/arm/std.h" -Dxdc_target_name__=GCArmv5T -Dxdc_cfg__header__="/home/caglar/myfs/work/tasks/source-codes/bilkon/lmm-demo-build-desktop-Qt_4_7_1__arm__Release/dm365_config/package/cfg/dm365_xv5T.h"
    SOURCES += \
        dmaiencoder.cpp \
        dm365dmaicapture.cpp \
        dm365camerainput.cpp \
        h264encoder.cpp \

    HEADERS += dmaiencoder.h \
        dm365dmaicapture.h \
        h264encoder.h \
        dm365camerainput.h \

    xdc.files += ../dm365.pri
    xdc.files += ../config.bld
    xdc.files += ../dm365.cfg
    xdc.files += ../xdc_linker.cmd
    xdc.path = /usr/local/include/lmm/dm365
}

dm6446 {
    include(../dm6446.pri)
    DEFINES += CONFIG_DM6446
    SOURCES += \
        dmaidecoder.cpp \
        fboutput.cpp \
        dvb/tsdemux.cpp \
        dvb/dvbutils.cpp \
        blec32tunerinput.cpp \
        blec32fboutput.cpp \

    HEADERS  += \
        dmaidecoder.h \
        fboutput.h \
        dvb/tsdemux.h \
        dvb/dvbutils.h \
        blec32tunerinput.h \
        blec32fboutput.h \

    xdc.files += ../xdc_linker.cmd
    xdc.path = /usr/local/share/lmm
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
    DEFINES += TARGET_ARM
}

headers.files = $$HEADERS lmm-arm.pri
headers.path = /usr/local/include/lmm

target.path = /usr/local/lib

INSTALLS += target headers xdc

OTHER_FILES +=
