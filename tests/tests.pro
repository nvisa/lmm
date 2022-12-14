TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= gui

QT += network

SOURCES += main.cpp \
    testcontroller.cpp \
    pipelinefixturegeneral.cpp

include (build_config.pri)

LIBS += $$GTEST_ROOT/make/gtest.a
INCLUDEPATH += $$GTEST_ROOT/include

lessThan(QT_VERSION, 4.7) {
    INCLUDEPATH += $$PWD/../lmm/compat
}

include($$PWD/../lmm/dm365/dm365_xdc.pri)

INCLUDEPATH += $$INSTALL_PREFIX/usr/local/include
LIBS += $$INSTALL_PREFIX/usr/local/lib/libEncoderCommonLibrary.a
PRE_TARGETDEPS += $$INSTALL_PREFIX/usr/local/lib/libEncoderCommonLibrary.a

DEPENDPATH += $${INCLUDEPATH}

HEADERS += \
    testcontroller.h \
    pipelinefixturegeneral.h \
    common.h
