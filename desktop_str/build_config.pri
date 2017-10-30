message("building default")
#This is our default build config
INSTALL_PREFIX=$$OUT_PWD/../../
DEFINES += DEBUG INFO LOG LOGV DEBUG_TIMING
INCLUDEPATH += $$PWD/../
LIBS += $$OUT_PWD/../lmm/liblmm.a
PRE_TARGETDEPS += $$OUT_PWD/../lmm/liblmm.a

LIBS += -lavformat -lswscale -lavcodec -lavutil -lX11 -lXext -lXcomposite -lx264 -lsrtp

#LIBS += -L$(SDKTARGETSYSROOT)/usr/lib/
#LIBS += -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lgstbase-1.0 -lgstapp-1.0
#LIBS += -L$(SDKTARGETSYSROOT)/usr/lib/gstreamer-1.0

#INCLUDEPATH += $(SDKTARGETSYSROOT)/usr/lib/glib-2.0/include
#INCLUDEPATH += $(SDKTARGETSYSROOT)/usr/include/glib-2.0
#INCLUDEPATH += $(SDKTARGETSYSROOT)/usr/lib/gstreamer-1.0/include
#INCLUDEPATH += $(SDKTARGETSYSROOT)/usr/include/gstreamer-1.0
