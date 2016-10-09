#-------------------------------------------------
#
# Project created by QtCreator 2016-02-25T16:04:03
#
#-------------------------------------------------

QT       += core network

QT       += gui

TARGET = dm365_ipstr
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    mpegdashserver.cpp \
    genericstreamer.cpp \
    jpegshotserver.cpp \
    simplezip.cpp \
    miniz.c \
    mjpegserver.cpp

include (build_config.pri)

lessThan(QT_VERSION, 4.7) {
    INCLUDEPATH += $$PWD/../lmm/compat
}

include($$PWD/../lmm/dm365/dm365_xdc.pri)

INCLUDEPATH += $$INSTALL_PREFIX/usr/local/include
LIBS += $$INSTALL_PREFIX/usr/local/lib/libEncoderCommonLibrary.a
PRE_TARGETDEPS += $$INSTALL_PREFIX/usr/local/lib/libEncoderCommonLibrary.a

DEPENDPATH += $${INCLUDEPATH}

HEADERS += \
    mpegdashserver.h \
    genericstreamer.h \
    jpegshotserver.h \
    simplezip.h \
    mjpegserver.h
