QT += core network
QT -= gui

TARGET = tk1_ipstr
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    uvcvideoinput.cpp \
    tk1videostreamer.cpp \
    tk1omxpipeline.cpp

include (build_config.pri)

HEADERS += \
    uvcvideoinput.h \
    tk1videostreamer.h \
    tk1omxpipeline.h

