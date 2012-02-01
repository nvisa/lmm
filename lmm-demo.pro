#-------------------------------------------------
#
# Project created by QtCreator 2012-01-13T13:52:36
#
#-------------------------------------------------

QT       += core gui sql

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = lmm demo

RESOURCES += \
    art.qrc

OTHER_FILES += \
    xdc_linker.cmd \
    dm6446.pri \
    dm365.pri \
    dm365.cfg \
    config.bld
