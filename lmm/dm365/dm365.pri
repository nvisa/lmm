include(tipaths.pri)

XDC_PATH = $${USER_XDC_PATH};$${DMAI_INSTALL_DIR}/packages;$${CE_INSTALL_DIR}/packages;$${FC_INSTALL_DIR}/packages;$${LINK_INSTALL_DIR}/packages;$${XDAIS_INSTALL_DIR}/packages;$${CMEM_INSTALL_DIR}/packages;$${CODEC_INSTALL_DIR}/packages;$${CE_INSTALL_DIR}/examples
XDC_CFGFILE = $$INSTALL_PREFIX/usr/local/include/lmm/dm365/dm365.cfg
XDC_CFG = dm365_config
XDC_CFLAGS = $${XDC_CFG}/compiler.opt
XDC_LFILE = $${XDC_CFG}/linker.cmd
XDC_PLATFORM = ti.platforms.evmDM365
XDC_TARGET = gnu.targets.arm.GCArmv5T
CONFIGURO = $${XDC_INSTALL_DIR}/xs xdc.tools.configuro
CONFIG_BLD = $$INSTALL_PREFIX/usr/local/include/lmm/dm365/config.bld

xdctarget.target = xdctarget
xdctarget.commands = XDCPATH=\"$${XDC_PATH}\" $${CONFIGURO} -o $${XDC_CFG} -t $${XDC_TARGET} -p $${XDC_PLATFORM} -b $${CONFIG_BLD} $${XDC_CFGFILE}
xdctarget.depends = $${XDC_CFGFILE}

QMAKE_EXTRA_TARGETS += xdctarget
PRE_TARGETDEPS += xdctarget

LIBS += -ldl -lrt

LIBS += \
    dm365_config/package/cfg/dm365_xv5T.ov5T \
    $${DVSDK_INSTALL_DIR}/codecs-dm365_4_02_00_00/packages/ittiam/codecs/aaclc_enc/ce/lib/resource.a470MV \
    $${DVSDK_INSTALL_DIR}/dmai_2_20_00_15/packages/ti/sdo/dmai/lib/dmai_linux_dm365.a470MV \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/image1/lib/release/imgdec1.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/image1/lib/release/imgenc1.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/image/lib/release/image.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/video2/lib/release/viddec2.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/video1/lib/release/viddec1.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/video1/lib/release/videnc1.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/video/lib/release/video.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/audio1/lib/release/auddec1.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/audio1/lib/release/audenc1.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/audio/lib/release/audio.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/speech1/lib/release/sphdec1.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/speech1/lib/release/sphenc1.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/speech/lib/release/speech.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/lib/release/ce.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/alg/lib/release/Algorithm_noOS.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/alg/lib/release/alg.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/ipc/linux/lib/release/ipc_linux.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/osal/linux/lib/release/osal_linux_470.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/acpy3/lib/release/acpy3.a470MV \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/dman3/lib/release/dman3Cfg.a470MV \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/node/lib/release/node.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/packages/ti/sdo/ce/utils/xdm/lib/release/XdmUtils.av5T \
    $${DVSDK_INSTALL_DIR}/codecs-dm365_4_02_00_00/packages/ti/sdo/codecs/jpegenc/lib/libjpgenc.a \
    $${DVSDK_INSTALL_DIR}/codecs-dm365_4_02_00_00/packages/ti/sdo/codecs/jpegenc/lib/libimx.a \
    $${DVSDK_INSTALL_DIR}/codecs-dm365_4_02_00_00/packages/ti/sdo/codecs/mpeg4enc/lib/libmp4enc.a \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/vicpsync/lib/release/vicpsync.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/ires/vicp/lib/release/vicp.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/ires/grouputils/lib/release/grouputils.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/ires/edma3chan/lib/release/edma3Chan.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/edma3/lib/release/edma3.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/rman/lib/release/rman.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/ires/nullresource/lib/release/nullres.av5T \
    $${DVSDK_INSTALL_DIR}/codecs-dm365_4_02_00_00/packages/ti/sdo/codecs/mpeg2enc/lib/mpeg2venc_ti_arm926.a \
    $${DVSDK_INSTALL_DIR}/codecs-dm365_4_02_00_00/packages/ti/sdo/codecs/mpeg2enc/lib/dma_ti_dm365.a \
    $${DVSDK_INSTALL_DIR}/codecs-dm365_4_02_00_00/packages/ti/sdo/codecs/h264enc/lib/h264venc_ti_arm926.a \
    $${DVSDK_INSTALL_DIR}/codecs-dm365_4_02_00_00/packages/ti/sdo/codecs/h264enc/lib/h264v_ti_dma_dm365.a \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/hdvicpsync/lib/release/hdvicpsync.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/utils/trace/lib/release/gt.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/ires/memtcm/lib/release/memtcm.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/ires/hdvicp/lib/release/hdvicp.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/utils/lib/release/rmm.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/utils/lib/release/smgr.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/utils/lib/release/rmmp.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/utils/lib/release/smgrmp.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/utils/lib/release/shm.av5T \
    $${DVSDK_INSTALL_DIR}/codecs-dm365_4_02_00_00/packages/ittiam/codecs/aaclc_enc/lib_production/aaclc_enc_prod.a \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/ires/addrspace/lib/release/addrspace.av5T \
    $${DVSDK_INSTALL_DIR}/framework-components_2_26_00_01/packages/ti/sdo/fc/memutils/lib/release/memutils.av5T \
    $${DVSDK_INSTALL_DIR}/codec-engine_2_26_02_11/examples/ti/xdais/dm/examples/g711/lib/release/g711.av5T \
    $${DVSDK_INSTALL_DIR}/linuxutils_2_26_01_02/packages/ti/sdo/linuxutils/cmem/lib/cmem.a470MV \
    $${DVSDK_INSTALL_DIR}/linuxutils_2_26_01_02/packages/ti/sdo/linuxutils/edma/lib/edma.a470MV \
    $${DVSDK_INSTALL_DIR}/linuxutils_2_26_01_02/packages/ti/sdo/linuxutils/vicp/lib/vicp.a470MV \
    $${DVSDK_INSTALL_DIR}/xdctools_3_16_03_36/packages/gnu/targets/arm/rtsv5T/lib/gnu.targets.arm.rtsv5T.av5T
