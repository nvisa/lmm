INCLUDEPATH += $$INSTALL_PREFIX/usr/local/include
OBJECTS += $$INSTALL_PREFIX/usr/local/lib/liblmm-arm.a
LIBS += $$INSTALL_PREFIX/usr/local/lib/liblmm-arm.a

include (build_config.pri)
dm6446 {
    LIBS += -Wl,-T,$$INSTALL_PREFIX/usr/local/share/lmm/xdc_linker.cmd
    LIBS += -lasound -lavformat -lmad -ltag
}
