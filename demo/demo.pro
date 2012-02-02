TARGET = lmm-demo
TEMPLATE = app

CONFIG += ffmpeg mad alsa dm6446

FORMS    += lmsdemo.ui

target.path = /usr/local/bin
INSTALLS += target

dm365 {
    include(../dm365.pri)
    QT -= gui
    SOURCES += main-dm365.cpp
}
dm6446 {
    include(../dm6446.pri)
    SOURCES += \
        main.cpp\
        lmsdemo.cpp

    HEADERS += lmsdemo.h
}

CROSS_COMPILE=$$(OE_QMAKE_CC)
isEmpty(CROSS_COMPILE) {
    CONFIG += x86
} else {
    CONFIG += arm
}

x86 {
    include(/home/caglar/myfs/work/tasks/source-codes/bilkon/build/usr/local/include/qtCommon.pri)
    include(/home/caglar/myfs/work/tasks/source-codes/bilkon/build/usr/local/include/lmm/lmm.pri)
    include(/home/caglar/myfs/work/tasks/source-codes/bilkon/build/usr/local/include/emdesk/emdeskCommon.pri)
}
arm {
    include(/home/caglar/myfs/work/tasks/source-codes/bilkon/build/usr/local/include/qtCommon-arm.pri)
    include(/home/caglar/myfs/work/tasks/source-codes/bilkon/build/usr/local/include/lmm/lmm-arm.pri)
    include(/home/caglar/myfs/work/tasks/source-codes/bilkon/build/usr/local/include/emdesk/emdeskCommon-arm.pri)
}
