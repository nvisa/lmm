QT += core network
QT += gui widgets

TARGET = desktop_str
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    desktopstreamer.cpp \
    x11videoinput.cpp

include (build_config.pri)

HEADERS += \
    desktopstreamer.h \
    x11videoinput.h


