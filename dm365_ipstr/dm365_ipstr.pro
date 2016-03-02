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

SOURCES += main.cpp

include (build_config.pri)

lessThan(QT_VERSION, 4.7) {
    INCLUDEPATH += $$INSTALL_PREFIX/usr/local/include/lmm/compat
}

include($$INSTALL_PREFIX/usr/local/include/lmm/lmm.pri)
include($$INSTALL_PREFIX/usr/local/include/lmm/dm365/dm365_xdc.pri)

DEPENDPATH += $${INCLUDEPATH}
