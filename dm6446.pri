SOURCES += ../xdc_runtime.c

TI_PATH = /home/caglar/myfs/work/tasks/openembedded/old/build/tmp/sysroots/bilkon-blec32-angstrom-linux-gnueabi/usr/share/ti/
INCLUDEPATH += "$${TI_PATH}/ti-xdctools-tree/packages" \
    "$${TI_PATH}/ti-dsplink-tree" \
    "$${TI_PATH}/ti-framework-components-tree/packages" \
    "$${TI_PATH}/ti-codec-engine-tree/packages" \
    "$${TI_PATH}/ti-xdais-tree/packages" \
    "$${TI_PATH}/ti-codecs-tree/packages" \
    "$${TI_PATH}/ti-linuxutils-tree/packages" \
    "$${TI_PATH}/ti-dmai-tree/packages" \
    "$${TI_PATH}/ti-local-power-manager-tree/packages" \
    "$${TI_PATH}/ti-edma3lld-tree/packages" \
    "$${TI_PATH}/ti-c6accel-tree/soc/c6accelw" \
    "$${TI_PATH}/ti-c6accel-tree/soc/packages" \
    "$${TI_PATH}/ti-xdctools-tree/packages"

QMAKE_CXXFLAGS += -march=armv5t -DPlatform_dm6446 \
    -Dxdc_target_types__="gnu/targets/arm/std.h" \
    -Dxdc_target_name__=GCArmv5T \
    -Dxdc_cfg__header__="xdc_config.h"
QMAKE_CFLAGS += -DPlatform_dm6446 \
    -Dxdc_target_types__="gnu/targets/arm/std.h" \
    -Dxdc_target_name__=GCArmv5T \
    -Dxdc_cfg__header__="xdc_config.h"

QMAKE_CXXFLAGS += -D__STDC_CONSTANT_MACROS
