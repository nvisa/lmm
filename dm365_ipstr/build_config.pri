message("building default")
#This is our default build config
INSTALL_PREFIX=$$OUT_PWD/../../
DEFINES += DEBUG INFO LOG LOGV DEBUG_TIMING
INCLUDEPATH += $$PWD/../
LIBS += $$OUT_PWD/../lmm/liblmm.a -lasound
PRE_TARGETDEPS += $$OUT_PWD/../lmm/liblmm.a
