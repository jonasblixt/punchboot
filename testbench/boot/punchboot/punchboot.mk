################################################################################
#
# punchboot
#
################################################################################

PUNCHBOOT_VERSION = $(shell cat $(PB_TESTBENCH_PUNCHBOOT_SRCDIR)/version.txt)
PUNCHBOOT_SITE = $(PB_TESTBENCH_PUNCHBOOT_SRCDIR)
PUNCHBOOT_SITE_METHOD = local
PUNCHBOOT_OVERRIDE_SRCDIR_RSYNC_EXCLUSIONS = $(PB_TESTBENCH_PUNCHBOOT_RSYNC_EXCLUSIONS)
PUNCHBOOT_LICENSE = BSD-3-Clause
PUNCHBOOT_LICENSE_FILES = LICENSE
PUNCHBOOT_INSTALL_TARGET = NO
PUNCHBOOT_INSTALL_IMAGES = YES
PUNCHBOOT_DEPENDENCIES = host-bpak host-python3 host-uboot-tools

PUNCHBOOT_BOARD = $(call qstrip,$(BR2_TARGET_PUNCHBOOT_BOARD))
PUNCHBOOT_CONFIG_FILE = $(or $(call qstrip,$(BR2_TARGET_PUNCHBOOT_CONFIG_FILE)),configs/$(PUNCHBOOT_BOARD)_defconfig)
PUNCHBOOT_BUILD_DIR = build-$(PUNCHBOOT_BOARD)

# The keystore and the HAB keys come from the punchboot configuration,
# by default the test keys in pki/ of the punchboot sources.
PUNCHBOOT_MAKE_OPTS = \
	BOARD=src/board/$(PUNCHBOOT_BOARD) \
	KCONFIG_CONFIG=$(PUNCHBOOT_CONFIG_FILE) \
	CROSS_COMPILE=$(TARGET_CROSS) \
	PYTHON=$(HOST_DIR)/bin/python3 \
	BPAK=$(HOST_DIR)/bin/bpak \
	MKIMAGE=$(HOST_DIR)/bin/mkimage \
	Q=

ifeq ($(BR2_TARGET_PUNCHBOOT_IMX_HAB),y)
PUNCHBOOT_DEPENDENCIES += host-imx-cst
PUNCHBOOT_MAKE_OPTS += CST_TOOL=$(HOST_DIR)/bin/cst
endif

# NXP names the files lpddr4_pmu_train_{1d,2d}_{imem,dmem}_<version>.bin,
# punchboot (src/plat/imx8m/ddr.c) wants them without the version. As in
# scripts/ci/build-firmware.sh the copies are under the build directory, the
# paths are relative to the punchboot sources.
ifeq ($(BR2_TARGET_PUNCHBOOT_IMX8M_DDR_FW),y)
PUNCHBOOT_DEPENDENCIES += firmware-imx
PUNCHBOOT_DDR_FW_DIR = $(PUNCHBOOT_BUILD_DIR)/ddr-fw
PUNCHBOOT_DDR_FW = 1d_imem 1d_dmem 2d_imem 2d_dmem
PUNCHBOOT_MAKE_OPTS += \
	IMX8M_UMCTL_TRAIN_1D_I=$(PUNCHBOOT_DDR_FW_DIR)/lpddr4_pmu_train_1d_imem.bin \
	IMX8M_UMCTL_TRAIN_1D_D=$(PUNCHBOOT_DDR_FW_DIR)/lpddr4_pmu_train_1d_dmem.bin \
	IMX8M_UMCTL_TRAIN_2D_I=$(PUNCHBOOT_DDR_FW_DIR)/lpddr4_pmu_train_2d_imem.bin \
	IMX8M_UMCTL_TRAIN_2D_D=$(PUNCHBOOT_DDR_FW_DIR)/lpddr4_pmu_train_2d_dmem.bin

define PUNCHBOOT_COPY_DDR_FW
	mkdir -p $(@D)/$(PUNCHBOOT_DDR_FW_DIR)
	$(foreach fw,$(PUNCHBOOT_DDR_FW), \
		cp $(FIRMWARE_IMX_DIR)/firmware/ddr/synopsys/lpddr4_pmu_train_$(fw)$(FIRMWARE_IMX_DDR_VERSION_SUFFIX).bin \
			$(@D)/$(PUNCHBOOT_DDR_FW_DIR)/lpddr4_pmu_train_$(fw).bin$(sep))
endef
PUNCHBOOT_PRE_BUILD_HOOKS += PUNCHBOOT_COPY_DDR_FW
endif

# The i.MX8X boot container: imx-seco installs the SECO firmware as
# ahab-container.img, imx-sc-firmware the boards' SCU firmware
ifeq ($(BR2_TARGET_PUNCHBOOT_IMX8X_CONTAINER),y)
PUNCHBOOT_DEPENDENCIES += host-imx-mkimage imx-sc-firmware imx-seco
PUNCHBOOT_MAKE_OPTS += \
	IMX8X_MKIMAGE=$(HOST_DIR)/bin/mkimage_imx8 \
	IMX8X_AHAB_IMAGE=$(BINARIES_DIR)/ahab-container.img \
	IMX8X_SCFW_IMAGE=$(BINARIES_DIR)/$(call qstrip,$(BR2_TARGET_PUNCHBOOT_IMX8X_SCFW))
endif

define PUNCHBOOT_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) $(PUNCHBOOT_MAKE_OPTS) all
endef

define PUNCHBOOT_INSTALL_IMAGES_CMDS
	$(foreach f,pb.bin pb.imx pb_signed.imx pb_signed_uuu.imx, \
		$(if $(wildcard $(@D)/$(PUNCHBOOT_BUILD_DIR)/$(f)), \
			$(INSTALL) -D -m 0644 $(@D)/$(PUNCHBOOT_BUILD_DIR)/$(f) \
				$(BINARIES_DIR)/punchboot/$(f)$(sep)))
endef

$(eval $(generic-package))
