include(tipaths.pri)


EZSDK = $${EZSDK_INSTALL_DIR}
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/omx/omxcore/lib/a8host/debug/omxcore.av5T
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/omx/domx/lib/a8host/debug/domx.av5T
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/omx/memcfg/lib/a8host/debug/memcfg.av5T
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/timmosal/lib/a8host/debug/timmosal.av5T
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/omx/domx/delegates/shmem/lib/a8host/debug/domx_delegates_shmem.av5T
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/sdo/xdcruntime/linux/lib/debug/osal_linux_470.av5T
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/linuxdist/build/lib/osal.a
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/linuxdist/cstubs/lib/cstubs.a
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/sdo/rcm/lib/debug/rcm_syslink.av5T
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/sdo/fc/memutils/lib/release/memutils.av5T
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/sdo/xdcruntime/linux/lib/debug/osal_linux_470.av5T
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/sdo/fc/global/lib/debug/fcsettings.av5T
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/syslink/lib/syslink.a_debug
#LIBS += $${EZSDK}/linux-devkit/arm-none-linux-gnueabi/usr/lib/ti/sdo/linuxutils/cmem/lib/cmem.a470MV
LIBS += $${EZSDK}/component-sources/omx_05_02_00_48/lib/omxcore.av5T
LIBS += $${EZSDK}/component-sources/omx_05_02_00_48/lib/memcfg.av5T
LIBS += $${EZSDK}/component-sources/omx_05_02_00_48/lib/timmosal.av5T
LIBS += $${EZSDK}/component-sources/omx_05_02_00_48/lib/domx.av5T
LIBS += $${EZSDK}/component-sources/omx_05_02_00_48/lib/domx_delegates_shmem.av5T
LIBS += $${EZSDK}/component-sources/omx_05_02_00_48/lib/omxcfg.av5T
LIBS += $${EZSDK}/component-sources/osal_1_22_01_09/packages/linuxdist/build/lib/osal.a
LIBS += $${EZSDK}/component-sources/osal_1_22_01_09/packages/linuxdist/cstubs/lib/cstubs.a
LIBS += $${EZSDK}/component-sources/framework_components_3_22_01_07/packages/ti/sdo/rcm/lib/release/rcm_syslink.av5T
LIBS += $${EZSDK}/component-sources/framework_components_3_22_01_07/packages/ti/sdo/fc/memutils/lib/release/memutils.av5T
LIBS += $${EZSDK}/component-sources/osal_1_22_01_09/packages/ti/sdo/xdcruntime/linux/lib/release/osal_linux_470.av5T
LIBS += $${EZSDK}/component-sources/framework_components_3_22_01_07/packages/ti/sdo/fc/global/lib/release/fcsettings.av5T
LIBS += $${EZSDK}/component-sources/syslink_2_20_02_20/packages/ti/syslink/lib/syslink.a_debug
LIBS += $${EZSDK}/component-sources/linuxutils_3_22_00_02/packages/ti/sdo/linuxutils/cmem/lib/cmem.a470MV
LIBS += $${EZSDK}/component-sources/uia_1_01_01_14/packages/ti/uia/linux/lib/servicemgr.a

QMAKE_LFLAGS += -Xlinker --start-group

#QMAKE_CXXFLAGS += -I"$${EZSDK_INSTALL_DIR}/linux-devkit/arm-none-linux-gnueabi/usr/include/ti/omx/interfaces/openMaxv11/"
LIBS += -lgstapp-0.10 -lgstinterfaces-0.10 -lgstreamer-0.10 -lgobject-2.0 -lgmodule-2.0 -lgthread-2.0 -lrt -lxml2 -lglib-2.0
