#-------------------------------------------------
#
# Project created by QtCreator 2012-01-13T13:52:36
#
#-------------------------------------------------

QT       += core gui sql

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = lmm

linux-gnueabi-oe-g++ {
    #This should be a dm365 build
    SUBDIRS += dm365_ipstr \
        tests
}
