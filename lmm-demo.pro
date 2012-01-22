#-------------------------------------------------
#
# Project created by QtCreator 2012-01-13T13:52:36
#
#-------------------------------------------------

QT       += core gui

TARGET = lmm-demo
TEMPLATE = app


SOURCES += main.cpp\
        lmsdemo.cpp \
    lmm/filesource.cpp \
    lmm/avidemux.cpp \
    lmm/rawbuffer.cpp \
    lmm/avidecoder.cpp \
    lmm/mad.cpp \
    lmm/alsa/alsa.cpp \
    lmm/alsaoutput.cpp \
    lmm/dmaidecoder.cpp \
    xdc_runtime.c \
    lmm/circularbuffer.cpp \
    lmm/fboutput.cpp \
    lmm/baselmmelement.cpp \
    lmm/baselmmdecoder.cpp \
    lmm/baselmmoutput.cpp \
    lmm/streamtime.cpp \
    lmm/baselmmplayer.cpp \
    lmm/baselmmdemux.cpp \
    lmm/v4l2input.cpp \
    lmm/dvbplayer.cpp \
    lmm/fileoutput.cpp \
    lmm/mpegtsdemux.cpp \
    lmm/dvb/tsdemux.cpp \
    lmm/dvb/dvbutils.cpp

HEADERS  += lmsdemo.h \
    lmm/filesource.h \
    lmm/avidemux.h \
    lmm/rawbuffer.h \
    lmm/avidecoder.h \
    lmm/mad.h \
    lmm/alsa/alsa.h \
    lmm/alsaoutput.h \
    lmm/dmaidecoder.h \
    lmm/circularbuffer.h \
    lmm/fboutput.h \
    lmm/baselmmelement.h \
    lmm/baselmmdecoder.h \
    lmm/baselmmoutput.h \
    lmm/streamtime.h \
    lmm/baselmmplayer.h \
    lmm/baselmmdemux.h \
    lmm/v4l2input.h \
    lmm/dvbplayer.h \
    lmm/fileoutput.h \
    lmm/mpegtsdemux.h \
    lmm/dvb/tsdemux.h \
    lmm/dvb/dvbutils.h

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

TI_PATH = /home/caglar/myfs/work/tasks/openembedded/old/build/tmp/sysroots/bilkon-blec32-angstrom-linux-gnueabi/usr/share/ti/
INCLUDEPATH += "$${TI_PATH}/ti-xdctools-tree/packages" \
    "$${TI_PATH}/ti-dsplink-tree" \
    "$${TI_PATH}/ti-framework-components-tree/packages" \
    "$${TI_PATH}/ti-codec-engine-tree/packages" \
    "$${TI_PATH}/ti-xdais-tree/packages" \
    "$${TI_PATH}/ti-codecs-tree/packages" \
    "$${TI_PATH}/ti-linuxutils-tree/packages" \
    "$${TI_PATH}/ti-dmai-tree/packages" \
    "$${TI_PATH}/ti-local-power-manager-tree/packages" \
    "$${TI_PATH}/ti-edma3lld-tree/packages" \
    "$${TI_PATH}/ti-c6accel-tree/soc/c6accelw" \
    "$${TI_PATH}/ti-c6accel-tree/soc/packages" \
    "$${TI_PATH}/ti-xdctools-tree/packages"

QMAKE_CXXFLAGS += -D__STDC_CONSTANT_MACROS -march=armv5t
QMAKE_CXXFLAGS += -march=armv5t -DPlatform_dm6446 \
    -Dxdc_target_types__="gnu/targets/arm/std.h" \
    -Dxdc_target_name__=GCArmv5T \
    -Dxdc_cfg__header__="xdc_config.h"
QMAKE_CFLAGS += -DPlatform_dm6446 \
    -Dxdc_target_types__="gnu/targets/arm/std.h" \
    -Dxdc_target_name__=GCArmv5T \
    -Dxdc_cfg__header__="xdc_config.h"


LIBS += -lasound -lavformat -lmad -ltag -Wl,-T,../lmm-demo/xdc_linker.cmd
LIBSsss += \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/utils/trace/lib/release/TraceUtil.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/bioslog/lib/release/bioslog.av5T \
    $${TI_PATH}/ti-dmai-tree/packages/ti/sdo/dmai/lib/dmai_linux_dm6446.a470MV \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/image/lib/release/image.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/video/lib/release/video.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/audio/lib/release/audio.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/speech/lib/release/speech.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/video1/lib/release/viddec1.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/video1/lib/release/videnc1.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/speech1/lib/release/sphdec1.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/speech1/lib/release/sphenc1.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/audio1/lib/release/auddec1.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/audio1/lib/release/audenc1.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/image1/lib/release/imgdec1.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/image1/lib/release/imgenc1.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/video2/lib/release/viddec2.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/lib/release/ce.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/alg/lib/release/Algorithm_noOS.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/alg/lib/release/alg.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/ipc/dsplink/lib/release/ipc_dsplink_6446.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/osal/linux/lib/release/osal_linux_470.av5T \
    $${TI_PATH}/ti-framework-components-tree/packages/ti/sdo/fc/acpy3/lib/release/acpy3.a470MV \
    $${TI_PATH}/ti-framework-components-tree/packages/ti/sdo/fc/dman3/lib/release/dman3Cfg.a470MV \
    $${TI_PATH}/ti-framework-components-tree/packages/ti/sdo/fc/utils/lib/release/rmm.av5T \
    $${TI_PATH}/ti-framework-components-tree/packages/ti/sdo/fc/utils/lib/release/smgr.av5T \
    $${TI_PATH}/ti-framework-components-tree/packages/ti/sdo/fc/utils/lib/release/rmmp.av5T \
    $${TI_PATH}/ti-framework-components-tree/packages/ti/sdo/fc/utils/lib/release/smgrmp.av5T \
    $${TI_PATH}/ti-framework-components-tree/packages/ti/sdo/fc/utils/lib/release/shm.av5T \
    $${TI_PATH}/ti-framework-components-tree/packages/ti/sdo/fc/memutils/lib/release/memutils.av5T \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/node/lib/release/node.av5T \
    $${TI_PATH}/ti-linuxutils-tree/packages/ti/sdo/linuxutils/cmem/lib/cmem.a470MV \
    $${TI_PATH}/ti-dsplink-tree/dsplink/gpp/export/BIN/Linux/DAVINCI/RELEASE/dsplink.lib \
    $${TI_PATH}/ti-codec-engine-tree/packages/ti/sdo/ce/utils/xdm/lib/release/XdmUtils.av5T \
    $${TI_PATH}/ti-framework-components-tree/packages/ti/sdo/utils/trace/lib/release/gt.av5T \
    $${TI_PATH}/ti-xdctools-tree/packages/gnu/targets/arm/rtsv5T/lib/gnu.targets.arm.rtsv5T.av5T

RESOURCES += \
    art.qrc

OTHER_FILES += \
    xdc_linker.cmd
