INSTALL_PREFIX=/home/caglar/myfs/work/tasks/source-codes/bilkon/build
INCLUDEPATH += $$INSTALL_PREFIX/usr/local/include
OBJECTS += $$INSTALL_PREFIX/usr/local/lib/liblmm-arm.a
LIBS += $$INSTALL_PREFIX/usr/local/lib/liblmm-arm.a
LIBS += -Wl,-T,$$INSTALL_PREFIX/usr/local/share/lmm/xdc_linker.cmd
LIBS += -lasound -lavformat -lmad -ltag
