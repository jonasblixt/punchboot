#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_IMX6UL

cflags-y += -mtune=cortex-a7
cflags-y += -DPLAT_VIRT_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DPLAT_PHY_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DMAX_XLAT_TABLES=32
cflags-y += -DMAX_MMAP_REGIONS=32

ldflags-y += -Tsrc/plat/imx6ul/link.lds

src-y  += src/plat/imx6ul/plat.c
src-y  += src/drivers/fuse/imx_hab_fuses.c
src-y  += src/plat/imx6ul/hab.c

src-y  += src/drivers/pinctrl/imx_iomuxc.c

# The image stages (platform.md 6.2, plat-imx.md 6). The inputs are set with
# ?=: the board's makefile.mk, read before this one, or the command line may
# give its own.
MKIMAGE ?= mkimage
CST_TOOL ?= cst
# The DCD and the boot data: the ROM's register writes before it loads pb
IMX6UL_IMAGE_CFG ?= $(BOARD)/imximage.cfg
# The keys, from the defconfig
IMX6UL_SRK_TABLE ?= $(call unquote,$(CONFIG_IMX6UL_SRK_TABLE))
IMX6UL_KEY_INDEX ?= $(CONFIG_IMX6UL_KEY_INDEX)
IMX6UL_SIGN_CERT ?= $(call unquote,$(CONFIG_IMX6UL_SIGN_CERT))
IMX6UL_IMAGE_SIGN_CERT ?= $(call unquote,$(CONFIG_IMX6UL_IMAGE_SIGN_CERT))

IMX6UL_CSF_TEMPLATE = src/plat/imx6ul/pb.csf.template
IMX6UL_UUU_CSF_TEMPLATE = src/plat/imx6ul/pb_uuu.csf.template

.PHONY: imx6ul_image imx6ul_sign_desc imx6ul_sign_image

# pb.imx: the IVT, the boot data and the DCD, and pb.bin. The ROM reads the
# IVT at PB_ENTRY - 0xC00.
imx6ul_image: $(BUILD_DIR)/pb.bin
	@$(MKIMAGE) -n $(IMX6UL_IMAGE_CFG) -T imximage -e $(CONFIG_PB_ENTRY) \
				-d $(BUILD_DIR)/pb.bin $(BUILD_DIR)/pb.imx

# pb.csf and pb_uuu.csf, HABv4 descriptions of pb.imx. pb.csf signs it from
# the IVT to its end. pb_uuu.csf also signs the DCD where the serial
# downloader (uuu) puts it, at 0x00910000: 0x2c bytes into pb.imx, as long
# as its header says (big endian, at byte 45).
imx6ul_sign_desc: imx6ul_image
	$(Q)imx=$(BUILD_DIR)/pb.imx; \
	 blocks="Blocks = $$(printf '0x%X 0x000 0x%X' $$(( $(CONFIG_PB_ENTRY) - 0xC00 )) $$(stat -c%s $$imx)) \"$$imx\""; \
	 uuu="Blocks = 0x00910000 0x02c 0x$$(dd if=$$imx bs=1 skip=45 count=2 2>/dev/null | xxd -p) \"$$imx\""; \
	 keys="s#__KEY_INDEX__#$(IMX6UL_KEY_INDEX)#g; s#__SRK_TBL__#$(IMX6UL_SRK_TABLE)#g; s#__CSFK_PEM__#$(IMX6UL_SIGN_CERT)#g; s#__IMG_PEM__#$(IMX6UL_IMAGE_SIGN_CERT)#g"; \
	 sed -e "s#__BLOCKS__#$$blocks#g" -e "$$keys" $(IMX6UL_CSF_TEMPLATE) > $(BUILD_DIR)/pb.csf && \
	 sed -e "s#__UUU_BLOCKS__#$$uuu#g" -e "s#__BLOCKS__#$$blocks#g" -e "$$keys" \
	     $(IMX6UL_UUU_CSF_TEMPLATE) > $(BUILD_DIR)/pb_uuu.csf

# pb_signed.imx for the boot media: two blocks of zeros, as the ROM reads
# the image from the third block, then pb.imx and its signature, padded to a
# block. pb_signed_uuu.imx for the serial downloader: pb.imx and the
# signature of pb_uuu.csf.
imx6ul_sign_image: imx6ul_sign_desc
	@$(CST_TOOL) --o $(BUILD_DIR)/pb_csf.bin --i $(BUILD_DIR)/pb.csf
	@cat $(BUILD_DIR)/pb.imx $(BUILD_DIR)/pb_csf.bin > $(BUILD_DIR)/pb_signed.imx_nopad
	@dd if=/dev/zero of=$(BUILD_DIR)/zero_pad.bin bs=512 count=2
	@cat $(BUILD_DIR)/zero_pad.bin $(BUILD_DIR)/pb_signed.imx_nopad > $(BUILD_DIR)/pb_signed.imx
	@truncate $(BUILD_DIR)/pb_signed.imx -s \%512
	@$(CST_TOOL) --o $(BUILD_DIR)/pb_csf_uuu.bin --i $(BUILD_DIR)/pb_uuu.csf
	@cat $(BUILD_DIR)/pb.imx $(BUILD_DIR)/pb_csf_uuu.bin > $(BUILD_DIR)/pb_signed_uuu.imx

image-$(CONFIG_IMX6UL_CREATE_IMX_IMAGE) += imx6ul_image
image-$(CONFIG_IMX6UL_SIGN_IMAGE) += imx6ul_sign_desc imx6ul_sign_image

# What `all` names as the build's output, unless the board names its own
ifdef CONFIG_IMX6UL_SIGN_IMAGE
FINAL_OUTPUT ?= $(BUILD_DIR)/pb_signed.imx
else ifdef CONFIG_IMX6UL_CREATE_IMX_IMAGE
FINAL_OUTPUT ?= $(BUILD_DIR)/pb.imx
endif
endif
