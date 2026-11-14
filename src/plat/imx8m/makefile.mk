#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_IMX8M_COMMON

# pb's buffers (platform.md 5.1), in the DRAM, after the DDR controller's state: the board's makefile.mk,
# read before this one, may put them elsewhere
PB_BUFFERS_BASE ?= 0x40060000
PB_BUFFERS_SIZE ?= 0x1000000

src-y  += src/plat/imx8m/plat.c
src-y  += src/plat/imx8m/ddr.c
src-$(CONFIG_PLAT_IMX8MQ) += src/plat/imx8m/plat_imx8mq.c
src-$(CONFIG_PLAT_IMX8MQ) += src/plat/imx8m/ddr_imx8mq.c
src-$(CONFIG_PLAT_IMX8MP) += src/plat/imx8m/plat_imx8mp.c
src-$(CONFIG_PLAT_IMX8MP) += src/plat/imx8m/ddr_imx8mp.c

# IMX8M_LOAD_BASE, IMX8M_LOAD_END: the ROM's load window, the on-chip RAM pb
# is loaded to and runs from. link.lds gives pb the window from the board's
# PB_ENTRY to its end (platform.md 5.1). IMX8M_OCRAM_SIZE: the OCRAM, to map
# and for link.lds, which lets the buffers be there.
# IMX8M_ROM_SIZE: the boot ROM to map, for the ROM's APIs (HAB).
ifdef CONFIG_PLAT_IMX8MQ
# Unchecked (plat-imx.md 5): what the link script had for the board in the
# tree, 224 KiB of the TCM from its entry
IMX8M_LOAD_BASE = 0x7e1000
IMX8M_LOAD_END = 0x819000
# What the i.MX8M Quad's BL31 takes isn't known, and no board of it has TF-A
PLAT_BL_PARAMS_LIMIT = 0xffffffff
IMX8M_OCRAM_SIZE = 0x20000
cflags-y += -DIMX8M_ROM_SIZE=0x20000
endif
# i.MX8M Plus: the OCRAM free area after the ROM's part (RM, Figure 6-2), up to 0x97ffff
ifdef CONFIG_PLAT_IMX8MP
IMX8M_LOAD_BASE = 0x920000
IMX8M_LOAD_END = 0x980000
PLAT_BL_PARAMS_LIMIT = 0x960000
IMX8M_OCRAM_SIZE = 0x90000
# The boot ROM is 252 KiB, then 4 KiB protected (RM, memory map)
cflags-y += -DIMX8M_ROM_SIZE=0x3f000
endif
ldflags-y += --defsym=IMX8M_LOAD_BASE=$(IMX8M_LOAD_BASE)
ldflags-y += --defsym=IMX8M_LOAD_END=$(IMX8M_LOAD_END)
ldflags-y += --defsym=IMX8M_OCRAM_SIZE=$(IMX8M_OCRAM_SIZE)
cflags-y += -DIMX8M_OCRAM_SIZE=$(IMX8M_OCRAM_SIZE)

# TF-A's bl_params (platform.md 5.3): BL31 takes them only from the OCRAM
# below PLAT_BL_PARAMS_LIMIT. plat.c has the memory, plat_bl_params, of
# PLAT_BL_PARAMS_SIZE bytes, and link.lds checks that it ends below the limit.
PLAT_BL_PARAMS_SIZE = 512
ifdef CONFIG_BOOT_ATF
cflags-y += -DPLAT_BL_PARAMS_SIZE=$(PLAT_BL_PARAMS_SIZE)
endif
ldflags-y += --defsym=PLAT_BL_PARAMS_SIZE=$(PLAT_BL_PARAMS_SIZE)
ldflags-y += --defsym=PLAT_BL_PARAMS_LIMIT=$(PLAT_BL_PARAMS_LIMIT)

cflags-y += -DPLAT_VIRT_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DPLAT_PHY_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DMAX_XLAT_TABLES=32
cflags-y += -DMAX_MMAP_REGIONS=32
ldflags-y += -Tsrc/plat/imx8m/link.lds

src-y  += src/drivers/pinctrl/imx_iomuxc.c

# The image stages (platform.md 6.2, plat-imx.md 6). The inputs are set with
# ?=: the board's makefile.mk, read before this one, or the command line may
# give its own.
ifdef CONFIG_PLAT_IMX8MQ
# NXP's mkimage_imx8m (imx-mkimage)
MKIMAGE ?= mkimage_imx8m
else
# U-Boot's mkimage
MKIMAGE ?= mkimage
endif
CST_TOOL ?= cst
# The i.MX8M Plus: the boot device in the image's header
IMX8M_BOOT_FROM ?= sd
# The keys, from the defconfig
IMX8M_SRK_TABLE ?= $(call unquote,$(CONFIG_IMX8M_SRK_TABLE))
IMX8M_KEY_INDEX ?= $(CONFIG_IMX8M_KEY_INDEX)
IMX8M_SIGN_CERT ?= $(call unquote,$(CONFIG_IMX8M_SIGN_CERT))
IMX8M_IMAGE_SIGN_CERT ?= $(call unquote,$(CONFIG_IMX8M_IMAGE_SIGN_CERT))

IMX8M_CSF_TEMPLATE = src/plat/imx8m/pb.csf.template

.PHONY: imx8m_image imx8m_sign_desc imx8m_sign_image

ifdef CONFIG_PLAT_IMX8MQ
# pb.imx: pb.bin padded to 512 bytes (pb_pad.bin), the loader of an image
# made with mkimage_imx8m. pb_pad.imx: pb.imx padded to 4 bytes, the image
# that is signed. The ROM reads the IVT at PB_ENTRY - 0x40.
imx8m_image: $(BUILD_DIR)/pb.bin
	@echo Generating $(BUILD_DIR)/pb.imx
	@dd if=$(BUILD_DIR)/pb.bin of=$(BUILD_DIR)/pb_pad.bin bs=512 conv=sync,noerror
	@$(MKIMAGE) -loader $(BUILD_DIR)/pb_pad.bin $(PB_ENTRY) -out $(BUILD_DIR)/pb.imx
	@dd if=$(BUILD_DIR)/pb.imx of=$(BUILD_DIR)/pb_pad.imx bs=4 conv=sync,noerror
	@echo "$(BUILD_DIR)/pb_pad.imx ready"

# pb.csf, the HABv4 description of pb_pad.imx: its first 0x200 bytes more
# than pb_pad.bin, from the IVT at PB_ENTRY - 0x40
imx8m_sign_desc: imx8m_image
	$(Q)blocks=$$(printf 'Blocks = 0x%X 0x0 0x%X' $$(( $(PB_ENTRY) - 0x40 )) \
	                $$(( $$(stat -c%s $(BUILD_DIR)/pb_pad.bin) + 0x200 ))); \
	 sed -e "s#__BLOCKS__#$$blocks \"$(BUILD_DIR)/pb_pad.imx\"#g" \
	     -e 's#__KEY_INDEX__#$(IMX8M_KEY_INDEX)#g' \
	     -e 's#__SRK_TBL__#$(IMX8M_SRK_TABLE)#g' \
	     -e 's#__CSFK_PEM__#$(IMX8M_SIGN_CERT)#g' \
	     -e 's#__IMG_PEM__#$(IMX8M_IMAGE_SIGN_CERT)#g' \
	     $(IMX8M_CSF_TEMPLATE) > $(BUILD_DIR)/pb.csf

# pb_signed.imx: pb_pad.imx with the signature in its last 0x2000 bytes,
# which mkimage_imx8m leaves for it
imx8m_sign_image: imx8m_sign_desc
	@$(CST_TOOL) -i $(BUILD_DIR)/pb.csf -o $(BUILD_DIR)/pb_csf.bin
	@cp $(BUILD_DIR)/pb_pad.imx $(BUILD_DIR)/pb_signed.imx
	$(Q)dd if=$(BUILD_DIR)/pb_csf.bin of=$(BUILD_DIR)/pb_signed.imx bs=1 conv=notrunc \
	       seek=$$(( $$(stat -c%s $(BUILD_DIR)/pb_pad.imx) - 0x2000 ))
else
# pb.imx: pb.bin with the header of the i.MX8M Plus ROM (ROM_VERSION v2), for
# the boot media or the serial downloader (uuu)
imx8m_image: $(BUILD_DIR)/pb.bin
	@echo Generating $(BUILD_DIR)/pb.imx
	@printf 'ROM_VERSION v2\nBOOT_FROM $(IMX8M_BOOT_FROM)\nLOADER $(BUILD_DIR)/pb.bin $(PB_ENTRY)\n' > $(BUILD_DIR)/pb.cfg
	@$(MKIMAGE) -n $(BUILD_DIR)/pb.cfg -T imx8mimage -e $(PB_ENTRY) -d $(BUILD_DIR)/pb.bin $(BUILD_DIR)/pb.imx > /dev/null
endif

image-$(CONFIG_IMX8M_CREATE_IMX_IMAGE) += imx8m_image
image-$(CONFIG_IMX8M_SIGN_IMAGE) += imx8m_sign_desc imx8m_sign_image

# What `all` names as the build's output, unless the board names its own
ifdef CONFIG_IMX8M_SIGN_IMAGE
FINAL_OUTPUT ?= $(BUILD_DIR)/pb_signed.imx
else ifdef CONFIG_IMX8M_CREATE_IMX_IMAGE
FINAL_OUTPUT ?= $(BUILD_DIR)/pb.imx
endif
endif
