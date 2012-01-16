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
    lmm/alsaoutput.cpp

HEADERS  += lmsdemo.h \
    lmm/filesource.h \
    lmm/avidemux.h \
    lmm/rawbuffer.h \
    lmm/avidecoder.h \
    lmm/mad.h \
    lmm/alsa/alsa.h \
    lmm/alsaoutput.h

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

QMAKE_CXXFLAGS = -D__STDC_CONSTANT_MACROS

LIBS += -lasound -lavformat -lmad -ltag
