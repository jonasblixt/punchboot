#
# Punch BOOT
#
# Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
#
# SPDX-License-Identifier: BSD-3-Clause
#
# i.MX8M Plus EVK. pb runs from OCRAM at 0x920000, after the ROM's part of it.

PB_ENTRY      = 0x920000

MKIMAGE ?= mkimage

cflags-y += -DBOARD_RAM_BASE=0x40000000
cflags-y += -DBOARD_RAM_END=0xc0000000

src-y += $(BOARD)/board.c
src-y += $(BOARD)/lpddr4_timing.c

# NXP's LPDDR4 training firmware FW2020.06 (the *_202006.bin files), under
# these names: src/plat/imx8m/ddr.c uses the symbols they give.
# scripts/ci/build-firmware.sh copies them from IMX8M_FW_DIR.
IMX8M_UMCTL_TRAIN_1D_D ?= lpddr4_pmu_train_1d_dmem.bin
IMX8M_UMCTL_TRAIN_1D_I ?= lpddr4_pmu_train_1d_imem.bin
IMX8M_UMCTL_TRAIN_2D_D ?= lpddr4_pmu_train_2d_dmem.bin
IMX8M_UMCTL_TRAIN_2D_I ?= lpddr4_pmu_train_2d_imem.bin

blobs-y  = $(IMX8M_UMCTL_TRAIN_1D_D) \
		   $(IMX8M_UMCTL_TRAIN_1D_I) \
		   $(IMX8M_UMCTL_TRAIN_2D_D) \
		   $(IMX8M_UMCTL_TRAIN_2D_I)

.PHONY: imx8mp_image

# Boot image for the ROM (ROM_VERSION v2), for SD/eMMC or USB serial
# download (uuu)
imx8mp_image: $(BUILD_DIR)/pb.bin
	@echo Generating $(BUILD_DIR)/pb.imx
	@printf 'ROM_VERSION v2\nBOOT_FROM sd\nLOADER $(BUILD_DIR)/pb.bin $(PB_ENTRY)\n' > $(BUILD_DIR)/pb.cfg
	@$(MKIMAGE) -n $(BUILD_DIR)/pb.cfg -T imx8mimage -e $(PB_ENTRY) -d $(BUILD_DIR)/pb.bin $(BUILD_DIR)/pb.imx > /dev/null

plat-y += imx8mp_image
