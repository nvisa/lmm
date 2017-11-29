QT += core network
QT -= gui

TARGET = tk1_ipstr
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    uvcvideoinput.cpp \
    tk1videostreamer.cpp \
    tk1omxpipeline.cpp \
    metadatamanager.cpp \
    net/tcpserver.cpp \
    net/udpserver.cpp \
    seiinserter.cpp \
    opencv/roi.cpp

include (build_config.pri)

HEADERS += \
    uvcvideoinput.h \
    tk1videostreamer.h \
    tk1omxpipeline.h \
    metadatamanager.h \
    net/tcpserver.h \
    net/udpserver.h \
    seiinserter.h \
    opencv/roi.h

via {
    INCLUDEPATH += $$VIA_REPO_PATH
    DEFINES += CONFIG_VIA
}

