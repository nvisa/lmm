# Define target platform.
PLATFORM=dm816x-evm
KERNEL_VERSION=2.6.37
DEFAULT_LINUXKERNEL_CONFIG=ti8168_evm_defconfig
DEFAULT_UBOOT_CONFIG=ti8168_evm_config
MIN_UBOOT_CONFIG=ti8168_evm_min_sd
SYSLINK_PLATFORM=TI816X
GRAPHICS_PLATFORM=6.x
OMX_PLATFORM=ti816x-evm
OMTB_PLATFORM=ti816x-evm
RPE_PLATFORM=ti816x-evm
MEDIA_CONTROLLER_UTILS_PLATFORM=ti816x-evm
MATRIX_PLATFORM=ti816x
EDMA3_LLD_TARGET=edma3_lld_ti816x-evm_674_libs

# The installation directory of the SDK.
EZSDK_INSTALL_DIR = $$(SDK_PATH)/../

# For backwards compatibility
DVSDK_INSTALL_DIR=$(EZSDK_INSTALL_DIR)

# For backwards compatibility
DVEVM_INSTALL_DIR=$(EZSDK_INSTALL_DIR)

# Where SYS/BIOS is installed.
SYSBIOS_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/bios_6_33_05_46

# Where the OSAL package is installed.
OSAL_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/osal_1_22_01_09

# OSAL build variables
OSAL_BUILD_VARS = CROSS_COMPILE=$(CSTOOL_PREFIX) \
		OSAL_INSTALL_DIR=$(OSAL_INSTALL_DIR)

# Where the IPC package is installed.
IPC_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/ipc_1_24_03_32

# Where the Codec Engine package is installed.
CE_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/codec_engine_3_22_01_06

# Where the SYS Link package is installed.
SYSLINK_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/syslink_2_20_02_20

# Syslink build variables
SYSLINK_BUILD_VARS = DEVICE=$(SYSLINK_PLATFORM) \
	GPPOS=Linux \
	LOADER=ELF \
	SDK=EZSDK \
	IPC_INSTALL_DIR="${IPC_INSTALL_DIR}" \
	BIOS_INSTALL_DIR="${SYSBIOS_INSTALL_DIR}" \
	XDC_INSTALL_DIR="${XDC_INSTALL_DIR}" \
	LINUXKERNEL="${LINUXKERNEL_INSTALL_DIR}" \
	CGT_ARM_INSTALL_DIR="${CSTOOL_DIR}" \
	CGT_ARM_PREFIX="${CSTOOL_PREFIX}" \
	CGT_C674_ELF_INSTALL_DIR="${CODEGEN_INSTALL_DIR}" \
	USE_SYSLINK_NOTIFY=0

# Where the EDMA3 LLD package is installed.
EDMA3_LLD_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/edma3lld_02_11_05_02
EDMA3LLD_INSTALL_DIR=$(EDMA3_LLD_INSTALL_DIR)

# EDMA3 LLD build variables
EDMA3_LLD_BUILD_VARS = ROOTDIR=$(EDMA3_LLD_INSTALL_DIR) \
		INTERNAL_SW_ROOT=$(EDMA3_LLD_INSTALL_DIR) \
		EXTERNAL_SW_ROOT=$(DVSDK_INSTALL_DIR) \
		edma3_lld_PATH=$(EDMA3_LLD_INSTALL_DIR) \
		bios_PATH=$(SYSBIOS_INSTALL_DIR) \
		xdc_PATH=$(XDC_INSTALL_DIR) \
		CODEGEN_PATH_DSPELF=$(CODEGEN_INSTALL_DIR) \
		FORMAT=ELF

# Where the Framework Components package is installed.
FC_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/framework_components_3_22_01_07

# Where the MFC Linux Utils package is installed.
LINUXUTILS_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/linuxutils_3_22_00_02
CMEM_INSTALL_DIR=$(LINUXUTILS_INSTALL_DIR)

# CMEM build variables
CMEM_BUILD_VARS = RULES_MAKE=../../../../../../../../../Rules.make

# Where the XDAIS package is installed.
XDAIS_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/xdais_7_22_00_03

# Where the RTSC tools package is installed.
XDC_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/xdctools_3_23_03_53

# Where the Code Gen is installed.
CODEGEN_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/dsp-devkit/cgt6x_7_3_4

# Where the PSP is installed.
PSP_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/board-support

# The directory that points to your kernel source directory.
LINUXKERNEL_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/board-support/linux-2.6.37-psp04.04.00.01
KERNEL_INSTALL_DIR=$(LINUXKERNEL_INSTALL_DIR)

# PSP Kernel/U-Boot build variables
LINUXKERNEL_BUILD_VARS = ARCH=arm CROSS_COMPILE=$(CSTOOL_PREFIX)
UBOOT_BUILD_VARS = CROSS_COMPILE=$(CSTOOL_PREFIX)

# Where opengl graphics package is installed.
GRAPHICS_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/graphics-sdk_4.04.00.02

# Graphics build variables
GRAPHICS_BUILD_VARS = BUILD=release \
	OMAPES=$(GRAPHICS_PLATFORM) \
	GRAPHICS_INSTALL_DIR=$(GRAPHICS_INSTALL_DIR) \
	CSTOOL_DIR=$(CSTOOL_DIR) \
	CROSS_COMPILE=$(CSTOOL_PREFIX) \
	KERNEL_INSTALL_DIR=$(LINUXKERNEL_INSTALL_DIR)

# Where the Unified Instrumentation Architecture is installed.
UIA_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/uia_1_01_01_14

# Where OMX package is installed
OMX_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/omx_05_02_00_48

# OMX build variables
OMX_BUILD_VARS = ROOTDIR=$(OMX_INSTALL_DIR) \
		EXTERNAL_SW_ROOT=$(DVSDK_INSTALL_DIR)/component-sources \
		INTERNAL_SW_ROOT=$(OMX_INSTALL_DIR)/src \
		kernel_PATH=${LINUXKERNEL_INSTALL_DIR} \
		bios_PATH=${SYSBIOS_INSTALL_DIR} \
		fc_PATH=$(FC_INSTALL_DIR) \
		osal_PATH=$(OSAL_INSTALL_DIR) \
		xdais_PATH=${XDAIS_INSTALL_DIR} \
		linuxutils_PATH=$(LINUXUTILS_INSTALL_DIR) \
		ce_PATH=${CE_INSTALL_DIR} \
		aaclcdec_PATH=${AACLCDEC_INSTALL_DIR} \
		ipc_PATH=$(IPC_INSTALL_DIR) \
		syslink_PATH=$(SYSLINK_INSTALL_DIR) \
		xdc_PATH=$(XDC_INSTALL_DIR) \
		uia_PATH=$(UIA_INSTALL_DIR) \
		slog_PATH=${SLOG_INSTALL_DIR} \
		lindevkit_PATH=${LINUX_DEVKIT_DIR}/arm-none-linux-gnueabi/usr \
		PLATFORM=$(OMX_PLATFORM) \
		EXAMPLES_ROOT=${OMX_INSTALL_DIR}/examples \
		CODEGEN_PATH_A8=$(CSTOOL_DIR) \
		CROSS_COMPILE=arm-none-linux-gnueabi- \
		TOOLCHAIN_LONGNAME=arm-none-linux-gnueabi \
		CODEGEN_PATH_DSPELF=${CODEGEN_INSTALL_DIR}

# Where OMTB package is installed
OMTB_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/example-applications/omtb_01_00_01_07

# OMTB build variables
OMTB_BUILD_VARS = ROOTDIR=$(OMTB_INSTALL_DIR) \
		PLATFORM=$(OMTB_PLATFORM) \
		OMTB_ROOT=$(OMTB_INSTALL_DIR) \
		EZSDK_INSTALL_DIR=$(DVSDK_INSTALL_DIR) \
		DEST_ROOT=$(OMTB_INSTALL_DIR)/bin \
		OMX_INSTALL_DIR=$(OMX_INSTALL_DIR)/src \
		fc_PATH=$(FC_INSTALL_DIR) \
		uia_PATH=$(UIA_INSTALL_DIR) \
		ce_PATH=$(CE_INSTALL_DIR) \
		osal_PATH=$(OSAL_INSTALL_DIR) \
		linuxutils_PATH=$(LINUXUTILS_INSTALL_DIR) \
		ipc_PATH=$(IPC_INSTALL_DIR) \
		syslink_PATH=$(SYSLINK_INSTALL_DIR) \
		xdc_PATH=$(XDC_INSTALL_DIR) \
		lindevkit_PATH=$(LINUX_DEVKIT_DIR)/arm-none-linux-gnueabi/usr \
		CROSS_COMPILE=arm-none-linux-gnueabi- \
		TOOLCHAIN_LONGNAME=arm-none-linux-gnueabi \
		CODEGEN_PATH_A8=$(CSTOOL_DIR)

# Where AAC-LC Decoder package is installed
AACLCDEC_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/c674x-aaclcdec_01_41_00_00_elf

# Where SLOG package is installed
SLOG_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/slog_04_00_00_02

# Where Media Controller Utils package is installed
MEDIA_CONTROLLER_UTILS_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/board-support/media-controller-utils_3_00_00_05

# Meda Controller Utils build variables
MEDIA_CONTROLLER_UTILS_BUILD_VARS = PLATFORM=$(MEDIA_CONTROLLER_UTILS_PLATFORM) \
		ROOTDIR=$(MEDIA_CONTROLLER_UTILS_INSTALL_DIR) \
		bios_PATH=${SYSBIOS_INSTALL_DIR} \
		ipc_PATH=$(IPC_INSTALL_DIR) \
		syslink_PATH=$(SYSLINK_INSTALL_DIR) \
		xdc_PATH=$(XDC_INSTALL_DIR) \
		DESTDIR=$(EXEC_DIR) prefix=/usr \
		CODEGEN_PATH_A8=$(CSTOOL_DIR) \
		CODEGEN_PATH_DSPELF=${CODEGEN_INSTALL_DIR}

GST_OMX_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/gst-openmax_GST_DM81XX_00_07_00_00

GST_OMX_BUILD_VARS = BIOS_INSTALL_DIR=$(SYSBIOS_INSTALL_DIR) \
		 IPC_INSTALL_DIR=$(IPC_INSTALL_DIR) \
		 XDC_INSTALL_DIR=$(XDC_INSTALL_DIR) \
		 FC_INSTALL_DIR=$(FC_INSTALL_DIR) \
		 CE_INSTALL_DIR=$(CE_INSTALL_DIR) \
		 OSAL_INSTALL_DIR=$(OSAL_INSTALL_DIR) \
		 UIA_INSTALL_DIR=$(UIA_INSTALL_DIR) \
		 SYSLINK_INSTALL_DIR=$(SYSLINK_INSTALL_DIR) \
		 LINUXUTILS_INSTALL_DIR=$(LINUXUTILS_INSTALL_DIR) \
		 OMX_INSTALL_DIR=$(OMX_INSTALL_DIR) \
		 LINUXKERNEL=$(LINUXKERNEL_INSTALL_DIR) \
		 KERNEL_INSTALL_DIR=$(LINUXKERNEL_INSTALL_DIR) \
		 OMX_PLATFORM=$(OMX_PLATFORM) \
		 CROSS_COMPILE=$(CSTOOL_PREFIX)

# Where RPE package is installed
RPE_INSTALL_DIR=$(DVSDK_INSTALL_DIR)/component-sources/rpe_1_00_01_13

# RPE build variables
RPE_BUILD_VARS = ROOTDIR=$(RPE_INSTALL_DIR) \
		EXTERNAL_SW_ROOT=$(DVSDK_INSTALL_DIR)/component-sources \
		INTERNAL_SW_ROOT=$(RPE_INSTALL_DIR)/src \
		bios_PATH=${SYSBIOS_INSTALL_DIR} \
		xdais_PATH=${XDAIS_INSTALL_DIR} \
		linuxutils_PATH=$(LINUXUTILS_INSTALL_DIR) \
		aaclcdec_PATH=${AACLCDEC_INSTALL_DIR} \
		ipc_PATH=$(IPC_INSTALL_DIR) \
		syslink_PATH=$(SYSLINK_INSTALL_DIR) \
		xdc_PATH=$(XDC_INSTALL_DIR) \
		uia_PATH=$(UIA_INSTALL_DIR) \
		mcutils_PATH=$(MEDIA_CONTROLLER_UTILS_INSTALL_DIR) \
		lindevkit_PATH=${LINUX_DEVKIT_DIR}/arm-none-linux-gnueabi/usr \
		RPE_USEAACDEC=1 \
		RPE_USEAACENC=0 \
		RPE_USEMP3DEC=0 \
		PLATFORM=$(RPE_PLATFORM) \
		CODEGEN_PATH_A8=$(CSTOOL_DIR) \
		CROSS_COMPILE=arm-none-linux-gnueabi- \
		TOOLCHAIN_LONGNAME=arm-none-linux-gnueabi \
		CODEGEN_PATH_DSPELF=${CODEGEN_INSTALL_DIR}

# The prefix to be added before the GNU compiler tools (optionally including # path), i.e. "arm_v5t_le-" or "/opt/bin/arm_v5t_le-".
CSTOOL_DIR=$$(TOOLCHAIN_PATH)
CSTOOL_LONGNAME=bin/arm-none-linux-gnueabi-gcc
CSTOOL_PREFIX=$(CSTOOL_DIR)/bin/arm-none-linux-gnueabi-
CSTOOL_PATH=$(CSTOOL_DIR)/bin

MVTOOL_DIR=$(CSTOOL_DIR)
MVTOOL_PREFIX=$(CSTOOL_PREFIX)

# Where the devkits are located
LINUX_DEVKIT_DIR=$(DVSDK_INSTALL_DIR)/linux-devkit
DSP_DEVKIT_DIR=$(DVSDK_INSTALL_DIR)/dsp-devkit

# Where to copy the resulting executables
EXEC_DIR=$(EZSDK_INSTALL_DIR)/filesystem/nfs//home/root/dm816x-evm

QMAKE_CXXFLAGS += -I"$${EZSDK_INSTALL_DIR}/linux-devkit/arm-none-linux-gnueabi/usr/include/ti/omx/interfaces/openMaxv11/" \
	-I"$${EZSDK_INSTALL_DIR}/linux-devkit/arm-none-linux-gnueabi/usr/include/gstreamer-0.10/" \
	-I"$${EZSDK_INSTALL_DIR}/linux-devkit/arm-none-linux-gnueabi/usr/include/libxml2/" \
	-I"$${EZSDK_INSTALL_DIR}/linux-devkit/arm-none-linux-gnueabi/usr/include/glib-2.0/" \
	-I"$${EZSDK_INSTALL_DIR}/linux-devkit/arm-none-linux-gnueabi/usr/lib/glib-2.0/include" \
	-I"$${EZSDK_INSTALL_DIR}/component-sources/xdctools_3_23_03_53/packages/" \
	-DMULTICHANNEL_OPT=1 -Dxdc_target_name__=GCArmv5T -Dxdc_target_types__=gnu/targets/arm/std.h  -Dxdc_bld__profile_debug  -Dxdc_bld__vers_1_0_4_3_3  -DGlobal_appTerminate=0 -D_VIDEO_M3_DYNAMIC_CONFIG -DGlobal_GrpxDssMsgHeapId=6 -DGlobal_TilerHeapId=7 -D_LOCAL_CORE_a8host_     -D_LOCAL_CORE_a8host_ -D_REMOTE_omxbase_ -D_REMOTE_vdec_ -D_REMOTE_server_ -D_REMOTE_scheduler_ -D_REMOTE_domx_ -D_REMOTE_domx_delegates_shmem_ -D_REMOTE_omxcore_ -D_REMOTE_omxbase_ -D_REMOTE_vfcc_ -D_REMOTE_vfpc_ -D_REMOTE_vfdc_ -D_REMOTE_ctrl_ -D_REMOTE_domx_ -D_REMOTE_domx_delegates_shmem_ -D_REMOTE_omxcore_ -D_BUILD_omxbase_ -D_BUILD_vdec_ -D_BUILD_server_ -D_BUILD_scheduler_ -D_BUILD_domx_ -D_BUILD_domx_delegates_shmem_ -D_BUILD_omxcore_ -D_BUILD_omxbase_ -D_BUILD_vfcc_ -D_BUILD_vfpc_ -D_BUILD_vfdc_ -D_BUILD_ctrl_ -D_BUILD_domx_ -D_BUILD_domx_delegates_shmem_ -D_BUILD_omxcore_  -DMAX_RESOLUTION_HD -DDOMX_CORE_REMOTEDUCATIHOST -DVC_APPS -DCODEC_H264DEC -DTI_816X_BUILD -DPLATFORM_EVM_SI -DADD_FBDEV_SUPPORT


