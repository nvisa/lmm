message("building TK default")
#This is our default build config
INSTALL_PREFIX=$$OUT_PWD/../../
DEFINES += DEBUG INFO LOG LOGV DEBUG_TIMING
INCLUDEPATH += $$PWD/../
LIBS += $$OUT_PWD/../lmm/liblmm.a
PRE_TARGETDEPS += $$OUT_PWD/../lmm/liblmm.a

LIBS += -L$(SDKTARGETSYSROOT)/usr/lib/
LIBS += -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lgstbase-1.0 -lgstapp-1.0
#ecl
INCLUDEPATH += $$INSTALL_PREFIX/usr/local/include
LIBS += $$INSTALL_PREFIX/usr/local/lib/libEncoderCommonLibrary.a
PRE_TARGETDEPS += $$INSTALL_PREFIX/usr/local/lib/libEncoderCommonLibrary.a

LIBS += -lavformat -lswscale -lavcodec -lavutil -lX11
#LIBS += -L$(SDKTARGETSYSROOT)/usr/lib/gstreamer-1.0

INCLUDEPATH += /usr/lib/arm-linux-gnueabihf/gstreamer-1.0/include
INCLUDEPATH += /usr/include/gstreamer-1.0/
INCLUDEPATH += /usr/lib/arm-linux-gnueabihf/glib-2.0/include
INCLUDEPATH += /usr/include/glib-2.0/

CONFIG += via
message("under the config")

LIBS += /home/ubuntu/myfs/caglar/VIA_ba/libgpu.a
LIBS += -L/usr/local/cuda/lib -lcudart -lcufft
INCLUDEPATH += /usr/include/opencv
LIBS += -lopencv_core \
    -lopencv_imgproc \
    -lopencv_highgui \
    -lopencv_ml \
    -lopencv_video \

#LIBS += -L/home/ubuntu/myfs/caglar/VIA_ba -lvideoAnalysis

#VIA_REPO_PATH=/home/ubuntu/myfs/caglar/VIA_ba

#LIBS += -L/home/amenmd/myfs/tasks/aselsan/VideoAnalytics/VIA/ -lasel_via
#LIBS += /home/amenmd/myfs/tasks/aselsan/VideoAnalytics/VIA/libgpu.a
#LIBS += -ldl -L/home/amenmd/myfs/tasks/aselsan/VideoAnalytics/VIA/libinstall/ -lcudart
