#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#

ifdef CONFIG_PLAT_IMX8X_COMMON

# pb's buffers (platform.md 5.1), in the DRAM: the board's makefile.mk,
# read before this one, may put them elsewhere
PB_BUFFERS_BASE ?= 0x85000000
PB_BUFFERS_SIZE ?= 0x1000000

src-y  += src/plat/imx8x/plat.c
src-y  += src/plat/imx8x/fusebox.c
src-y  += src/plat/imx8x/slc_helpers.c
src-y  += src/plat/imx8x/rot_helpers.c
src-y  += src/plat/imx8x/seco_log.c
src-y  += src/plat/imx8x/sci/ipc.c
src-y  += src/plat/imx8x/sci/imx8_mu.c
src-y  += src/plat/imx8x/sci/svc/pad/pad_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/pm/pm_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/timer/timer_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/misc/misc_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/seco/seco_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/rm/rm_rpc_clnt.c

asm-y += src/plat/imx8x/reset_vector.S

cflags-y += -mtune=cortex-a35
cflags-y += -DPLAT_VIRT_ADDR_SPACE_SIZE=0x200000000
cflags-y += -DPLAT_PHY_ADDR_SPACE_SIZE=0x200000000
cflags-y += -DMAX_XLAT_TABLES=32
cflags-y += -DMAX_MMAP_REGIONS=32
cflags-y += -Iinclude/plat/imx8x/

ldflags-y += -Tsrc/plat/imx8x/link.lds

# The image stages (platform.md 6.2, plat-imx.md 6). The inputs are set with
# ?=: the board's makefile.mk, read before this one, or the command line may
# give its own.
IMX8X_MKIMAGE ?= mkimage_imx8
CST_TOOL ?= cst
# The SECO firmware, the SoC's. The SCU firmware, IMX8X_SCFW_IMAGE, is the
# board's.
ifdef CONFIG_PLAT_IMX8XL
IMX8X_SOC = DXL
IMX8X_AHAB_IMAGE ?= mx8dxlb0-ahab-container.img
else
IMX8X_SOC = QX
IMX8X_AHAB_IMAGE ?= mx8qx-ahab-container.img
endif
# The boot device, as mkimage_imx8 names it
IMX8X_BOOT_DEVICE ?= emmc_fast
# The keys, from the defconfig
IMX8X_SRK_TABLE ?= $(call unquote,$(CONFIG_IMX8X_SRK_TABLE))
IMX8X_SIGN_CERT ?= $(call unquote,$(CONFIG_IMX8X_SIGN_CERT))
IMX8X_KEY_INDEX ?= $(CONFIG_IMX8X_KEY_INDEX)
IMX8X_KEY_REVOKE_MASK ?= $(CONFIG_IMX8X_KEY_REVOKE_MASK)

IMX8X_CSF_TEMPLATE = src/plat/imx8x/pb.csf.template

.PHONY: imx8x_image imx8x_sign_desc imx8x_sign_image

# pb.imx: the boot container with the SECO firmware, the SCU firmware and
# pb for the A35 at PB_ENTRY. pb.bin gets mkimage_imx8's commit id after it.
imx8x_image: $(BUILD_DIR)/pb.bin
	@echo Using SCFW image: $$(readlink -f $(IMX8X_SCFW_IMAGE))
	@echo Using AHAB image: $$(readlink -f $(IMX8X_AHAB_IMAGE))
	@$(IMX8X_MKIMAGE) -commit > $(BUILD_DIR)/head.hash
	@cat $(BUILD_DIR)/pb.bin $(BUILD_DIR)/head.hash > $(BUILD_DIR)/pb_hash.bin
	$(Q)$(IMX8X_MKIMAGE) -soc $(IMX8X_SOC) -rev B0 \
				  -e $(IMX8X_BOOT_DEVICE) \
				  -append $(IMX8X_AHAB_IMAGE) \
				  -c -scfw $(IMX8X_SCFW_IMAGE) \
				  -ap $(BUILD_DIR)/pb_hash.bin a35 $(PB_ENTRY) \
				  -out $(BUILD_DIR)/pb.imx

ifdef CONFIG_PLAT_IMX8XL
# Signing the i.MX8DXL's container isn't supported yet: the stage stops the
# build
imx8x_sign_desc: imx8x_image
	@echo Signing not yet supported on imx8dxl
	@exit 1
else
# pb.csf, the AHAB description of pb.imx: the container header at 0x400,
# its signature block at 0x590, as mkimage_imx8 places them on the 8QXP
imx8x_sign_desc: imx8x_image
	@echo Using SRK Table: $$(readlink -f $(IMX8X_SRK_TABLE))
	@echo Using signing cert: $$(readlink -f $(IMX8X_SIGN_CERT))
	@sed -e 's#__KEY_INDEX__#$(IMX8X_KEY_INDEX)#g' \
	     -e 's#__KEY_REVOKE_MASK__#$(IMX8X_KEY_REVOKE_MASK)#g' \
	     -e 's#__SRK_TBL__#$(IMX8X_SRK_TABLE)#g' \
	     -e 's#__CSFK_PEM__#$(IMX8X_SIGN_CERT)#g' \
	     -e 's#__FILE__#$(BUILD_DIR)/pb.imx#g' \
	     $(IMX8X_CSF_TEMPLATE) > $(BUILD_DIR)/pb.csf
endif

# pb_signed.imx: pb.imx with its signature, which cst writes
imx8x_sign_image: imx8x_sign_desc
	@$(CST_TOOL) -i $(BUILD_DIR)/pb.csf -o $(BUILD_DIR)/pb_signed.imx > /dev/null

image-$(CONFIG_IMX8X_CREATE_IMX_IMAGE) += imx8x_image
image-$(CONFIG_IMX8X_SIGN_IMAGE) += imx8x_sign_desc imx8x_sign_image

# What `all` names as the build's output, unless the board names its own
ifdef CONFIG_IMX8X_SIGN_IMAGE
FINAL_OUTPUT ?= $(BUILD_DIR)/pb_signed.imx
else ifdef CONFIG_IMX8X_CREATE_IMX_IMAGE
FINAL_OUTPUT ?= $(BUILD_DIR)/pb.imx
endif

endif
