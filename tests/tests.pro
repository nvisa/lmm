TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= gui

QT += network

SOURCES += main.cpp \
    testcontroller.cpp \
    pipelinefixturegeneral.cpp \
    btcpsocket.cpp \
    btcpserver.cpp

include (build_config.pri)

LIBS += $$GTEST_ROOT/make/gtest.a
INCLUDEPATH += $$GTEST_ROOT/include

lessThan(QT_VERSION, 4.7) {
    INCLUDEPATH += $$INSTALL_PREFIX/usr/local/include/lmm/compat
}

include($$INSTALL_PREFIX/usr/local/include/lmm/lmm.pri)
include($$INSTALL_PREFIX/usr/local/include/lmm/dm365/dm365_xdc.pri)

DEPENDPATH += $${INCLUDEPATH}

HEADERS += \
    testcontroller.h \
    pipelinefixturegeneral.h \
    common.h \
    btcpsocket.h \
    btcpserver.h
