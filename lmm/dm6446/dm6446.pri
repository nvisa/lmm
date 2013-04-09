include(tipaths.pri)

XDC_PATH = $${USER_XDC_PATH};$${DMAI_INSTALL_DIR}/packages;$${CE_INSTALL_DIR}/packages;$${FC_INSTALL_DIR}/packages;$${LINK_INSTALL_DIR}/packages;$${XDAIS_INSTALL_DIR}/packages;$${CMEM_INSTALL_DIR}/packages;$${CODEC_INSTALL_DIR}/packages;$${CE_INSTALL_DIR}/examples;$${DSPLINK_INSTALL_TREE};$${DSPBIOS_INSTALL_TREE}/packages;$${BIOSUTILS_INSTALL_TREE}/packages
XDC_CFGFILE = $$INSTALL_PREFIX/usr/local/include/lmm/dm6446/dm6446.cfg
XDC_CFG = dm6446_config
XDC_CFLAGS = $${XDC_CFG}/compiler.opt
XDC_LFILE = $${XDC_CFG}/linker.cmd
XDC_PLATFORM = ti.platforms.evmDM6446
XDC_TARGET = gnu.targets.arm.GCArmv5T
CONFIGURO = $${XDC_INSTALL_DIR}/xs xdc.tools.configuro
CONFIG_BLD = $$INSTALL_PREFIX/usr/local/include/lmm/dm6446/config.bld

xdctarget.target = xdctarget
xdctarget.commands = XDCPATH=\"$${XDC_PATH}\" $${CONFIGURO} -o $${XDC_CFG} -t $${XDC_TARGET} -p $${XDC_PLATFORM} -b $${CONFIG_BLD} $${XDC_CFGFILE}
xdctarget.depends = $${XDC_CFGFILE}

QMAKE_EXTRA_TARGETS += xdctarget
PRE_TARGETDEPS += xdctarget

LIBS += -ldl -lrt
LIBS += $$DMAI_INSTALL_DIR/packages/ti/sdo/dmai/lib/dmai_linux_dm6446.a470MV

QMAKE_LFLAGS += -Wl,-T\"$$OUT_PWD/$$XDC_CFG/linker.cmd\"
