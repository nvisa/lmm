PLATFORM=dm6446
TI_PATH=/home/caglar/myfs/work/tasks/blec32_sdk/ti/

# Where the Codec Engine package is installed.
CE_INSTALL_DIR=$${TI_PATH}/ti-codec-engine-tree

# Where the codecs are installed.
CODEC_INSTALL_DIR=$${TI_PATH}/ti-codecs-tree

# Where DMAI package is installed.
DMAI_INSTALL_DIR=$${TI_PATH}/ti-dmai-tree

DSPLINK_INSTALL_TREE=$${TI_PATH}/ti-dsplink-tree

DSPBIOS_INSTALL_TREE=$${TI_PATH}/ti-dspbios-tree

BIOSUTILS_INSTALL_TREE=$${TI_PATH}/ti-biosutils-tree

CODEGEN_INSTALL_DIR=$${TI_PATH}/ti-cgt6x-tree

# Where the Framework Components package is installed.
FC_INSTALL_DIR=$${TI_PATH}/ti-framework-components-tree

# Where the MFC Linux Utils package is installed.
LINUXUTILS_INSTALL_DIR=$${TI_PATH}/ti-linuxutils-tree
CMEM_INSTALL_DIR=$${LINUXUTILS_INSTALL_DIR}

# Where the XDAIS package is installed.
XDAIS_INSTALL_DIR=$${TI_PATH}/ti-xdais-tree

# Where the RTSC tools package is installed.
XDC_INSTALL_DIR=$${TI_PATH}/ti-xdctools-tree

LINUXKERNEL_INSTALL_DIR=$${TI_PATH}/blec_oe

QMAKE_CXXFLAGS += -march=armv5t -I"$${DMAI_INSTALL_DIR}/packages" \
    -I"$${CE_INSTALL_DIR}/packages" \
    -I"$${FC_INSTALL_DIR}/packages" \
    -I"/packages" -I"$${XDAIS_INSTALL_DIR}/packages" \
    -I"$${LINUXUTILS_INSTALL_DIR}/packages" \
    -I"$${CODEC_INSTALL_DIR}/packages" \
    -I"$${CE_INSTALL_DIR}/examples" \
    -I"$${XDC_INSTALL_DIR}/packages" \
    -I"$${DSPBIOS_INSTALL_DIR}/packages" \
    -I"$${BIOSUTILS_INSTALL_TREE}/packages" \
    -I"$${DSPLINK_INSTALL_DIR}" \
    -I"$${LINUXKERNEL_INSTALL_DIR}/include" \
    -I"$${LINUXKERNEL_INSTALL_DIR}/arch/arm/mach-davinci/include" \
    -Dxdc_target_types__="gnu/targets/arm/std.h" -Dxdc_target_name__=GCArmv5T \
    -DPlatform_dm6446
