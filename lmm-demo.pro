#-------------------------------------------------
#
# Project created by QtCreator 2012-01-13T13:52:36
#
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = lmm

linux-gnueabi-oe-g++ {
    message(DM365 build)
    #This should be a dm365 build
    SUBDIRS += dm365_ipstr \
        tests
}

linux-g++-64 {
    message(x86 build)
}

qmake-platform-k1 {
    message(Jetson TK1 native build)
    SUBDIRS += tk1_ipstr
}

qmake-platform-cross-k1 {
    message(Apalis TK1 build)
    SUBDIRS += tk1_ipstr
}
