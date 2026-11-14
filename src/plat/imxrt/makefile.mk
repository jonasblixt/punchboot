#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_IMXRT

# pb's buffers (platform.md 5.1), in the OCRAM, after pb: the board's makefile.mk,
# read before this one, may put them elsewhere
PB_BUFFERS_BASE ?= 0x20289000
PB_BUFFERS_SIZE ?= 0x37000

cflags-y += -mtune=cortex-m7

ldflags-y += -Tsrc/plat/imxrt/link.lds

src-y  += src/plat/imxrt/plat.c
src-y += src/drivers/fuse/imx_hab_fuses.c

src-y  += src/drivers/pinctrl/imx_iomuxc.c

# The image stages (platform.md 6.2, plat-imx.md 6). The inputs are set with
# ?=: the board's makefile.mk, read before this one, or the command line may
# give its own.
MKIMAGE ?= mkimage
# The image header's configuration: the boot device, the DCD if any
IMXRT_IMAGE_CFG ?= $(BOARD)/imximage.cfg
# The board's FlexSPI configuration block, a C file with 512 bytes in .fcb.
# No default: the flash image needs one.
IMXRT_FCB_SRC ?=

.PHONY: imxrt_image imxrt_flash_image

# pb.imx: the IVT and the boot data, and pb.bin
imxrt_image: $(BUILD_DIR)/pb.bin
	@$(MKIMAGE) -n $(IMXRT_IMAGE_CFG) -T imximage -e $(PB_ENTRY) \
				-d $(BUILD_DIR)/pb.bin $(BUILD_DIR)/pb.imx

$(BUILD_DIR)/fcb.bin: $(IMXRT_FCB_SRC)
	@echo FCB $<
	$(Q)$(CC) -c $(cflags-y) $< -o $(BUILD_DIR)/fcb.o
	$(Q)$(OBJCOPY) -O binary -j .fcb $(BUILD_DIR)/fcb.o $@
	$(Q)test $$(stat -c %s $@) -eq 512

# pb_flash.bin, what goes in the FlexSPI NOR from offset 0: the FCB, zeros
# up to the IVT at 0x1000 (RM 9.7.1), then pb.imx. The boot ROM copies it
# to the OCRAM.
imxrt_flash_image: imxrt_image $(BUILD_DIR)/fcb.bin
	$(Q)cp $(BUILD_DIR)/fcb.bin $(BUILD_DIR)/pb_flash.bin
	$(Q)truncate -s 4096 $(BUILD_DIR)/pb_flash.bin
	$(Q)cat $(BUILD_DIR)/pb.imx >> $(BUILD_DIR)/pb_flash.bin

image-$(CONFIG_IMXRT_CREATE_IMX_IMAGE) += imxrt_image
image-$(CONFIG_IMXRT_CREATE_FLASH_IMAGE) += imxrt_flash_image

# What `all` names as the build's output, unless the board names its own
ifdef CONFIG_IMXRT_CREATE_FLASH_IMAGE
FINAL_OUTPUT ?= $(BUILD_DIR)/pb_flash.bin
else ifdef CONFIG_IMXRT_CREATE_IMX_IMAGE
FINAL_OUTPUT ?= $(BUILD_DIR)/pb.imx
endif
endif
