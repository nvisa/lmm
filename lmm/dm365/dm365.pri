include(tipaths.pri)
DEFINES += CONFIG_DM365
SOURCES += \
    dm365/dm365camerainput.cpp \
    dm365/dm365videooutput.cpp \
    dm365/platformcommondm365.cpp \
    dm365/dm365dmacopy.cpp \
    dm365/vicp.c \
    dm365/SemMP_posix.c \
    dm365/ipcamerastreamer.cpp \
    dm365/simple1080pstreamer.cpp \
    dm365/simplertpstreamer.cpp \
    dm365/cvbsstreamer.cpp

HEADERS += dm365/dm365camerainput.h \
    dm365/dm365videooutput.h \
    dm365/platformcommondm365.h \
    dm365/dm365dmacopy.h \
    dm365/vicp.h \
    dm365/irqk.h \
    dm365/ipcamerastreamer.h \
    dm365/simple1080pstreamer.h \
    dm365/simplertpstreamer.h \
    dm365/cvbsstreamer.h

xdc.files += dm365/tipaths.pri
xdc.files += dm365/dm365_xdc.pri
xdc.files += dm365/config.bld
xdc.files += dm365/dm365.cfg
xdc.files += dm365/ih264venc.h
xdc.path = $$INSTALL_PREFIX/usr/local/include/lmm/dm365

INSTALLS += xdc

CONFIG += arm

OTHER_FILES += \
    dm365_xdc.pri \
    dm365.cfg \
    config.bld
