INCLUDEPATH += $$INSTALL_PREFIX/usr/local/include
OBJECTS += $$INSTALL_PREFIX/usr/local/lib/liblmm.a
LIBS += $$INSTALL_PREFIX/usr/local/lib/liblmm.a

dm6446 {
    LIBS += -Wl,-T,$$INSTALL_PREFIX/usr/local/share/lmm/xdc_linker.cmd
    LIBS += -lasound -lavformat -lmad -ltag
}

gstreamer {
    DEFINES += USE_GSTREAMER
    GST_CFLAGS = $$system(pkg-config gstreamer-0.10 --cflags-only-I | sed 's/-I//g')
    GST_LIBS = -lgstreamer-0.10 -lgobject-2.0 -lgmodule-2.0 -lxml2 -lgthread-2.0 -lrt -lglib-2.0 -lgstinterfaces-0.10
    INCLUDEPATH += $$GST_CFLAGS
    LIBS += $$GST_LIBS -lgstapp-0.10
}

ffmpeg {
	LIBS += -lavformat
	DEFINES += CONFIG_FFMPEG
}

vlc {
	LIBS += -lvlc
	DEFINES += CONFIG_VLC
}

live555 {
	LIBS += -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment
}
