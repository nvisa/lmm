message("building default")
#This is our default build config
INSTALL_PREFIX=$$OUT_PWD/../../
DEFINES += DEBUG INFO LOG LOGV DEBUG_TIMING
INCLUDEPATH += $$PWD/../
LIBS += $$OUT_PWD/../lmm/liblmm.a
PRE_TARGETDEPS += $$OUT_PWD/../lmm/liblmm.a

LIBS += -L$(SDKTARGETSYSROOT)/usr/lib/
LIBS += -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lgstbase-1.0 -lgstapp-1.0
#LIBS += -L$(SDKTARGETSYSROOT)/usr/lib/gstreamer-1.0

INCLUDEPATH += $(SDKTARGETSYSROOT)/usr/lib/glib-2.0/include
INCLUDEPATH += $(SDKTARGETSYSROOT)/usr/include/glib-2.0
INCLUDEPATH += $(SDKTARGETSYSROOT)/usr/lib/gstreamer-1.0/include
INCLUDEPATH += $(SDKTARGETSYSROOT)/usr/include/gstreamer-1.0

CONFIG += via
#LIBS += -L/home/amenmd/myfs/tasks/aselsan/VideoAnalytics/VIA/ -lasel_via
#LIBS += /home/amenmd/myfs/tasks/aselsan/VideoAnalytics/VIA/libgpu.a
LIBS += -ldl -L/home/amenmd/myfs/tasks/aselsan/VideoAnalytics/VIA/libinstall/ -lcudart
VIA_REPO_PATH=/home/amenmd/myfs/tasks/aselsan/VideoAnalytics/VIA/
