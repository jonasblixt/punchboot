#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#

ifdef CONFIG_PLAT_IMX8X

CST_TOOL ?= cst
MKIMAGE ?= mkimage_imx8x
IMX8X_AHAB_IMAGE ?= mx8qx-ahab-container.img
IMX8X_SCFW_IMAGE ?= scfw_tcm.bin
IMX8X_SRK_TABLE ?= pki/imx8x_ahab/crts/SRK_1_2_3_4_table.bin
IMX8X_SIGN_CERT ?= pki/imx8x_ahab/crts/SRK1_sha384_secp384r1_v3_usr_crt.pem
IMX8X_KEY_INDEX ?= 0
IMX8X_KEY_REVOKE_MASK ?= 0
PB_CSF_TEMPLATE = src/plat/imx8x/pb.csf.template

SED = $(shell which sed)

src-y  += src/plat/imx/usdhc.c
src-y  += src/plat/imx/gpt.c
src-y  += src/plat/imx/lpuart.c
src-y  += src/plat/imx/ehci.c
src-y  += src/plat/imx/caam.c
src-y  += src/plat/imx/gpio.c
src-y  += src/plat/imx8x/plat.c
src-y  += src/plat/imx/wdog.c
src-y  += src/plat/imx/usbdcd.c
src-y  += src/plat/imx8x/sci/ipc.c
src-y  += src/plat/imx8x/sci/imx8_mu.c
src-y  += src/plat/imx8x/sci/svc/pad/pad_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/pm/pm_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/timer/timer_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/misc/misc_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/seco/seco_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/rm/rm_rpc_clnt.c

asm-y += src/plat/imx8x/reset_vector.S

cflags-y += -I src/plat/imx8x/include
cflags-y += -mtune=cortex-a35

ldflags-y += -Tsrc/plat/imx8x/link.lds

.PHONY: imx8x_image imx8x_sign_image

imx8x_image: $(BUILD_DIR)/$(TARGET).bin
	@echo Using SCFW image: $(shell readlink -f $(IMX8X_SCFW_IMAGE))
	@echo Using AHAB image: $(shell readlink -f $(IMX8X_AHAB_IMAGE))
	@echo Using SRK Table: $(shell readlink -f $(IMX8X_SRK_TABLE))
	@echo Using signing cert: $(shell readlink -f $(IMX8X_SIGN_CERT))
	@$(MKIMAGE) -commit > $(BUILD_DIR)/head.hash
	@cat $(BUILD_DIR)/pb.bin $(BUILD_DIR)/head.hash > $(BUILD_DIR)/pb_hash.bin
	@$(MKIMAGE) -soc QX -rev B0 \
				  -e emmc_fast \
				  -append $(IMX8X_AHAB_IMAGE) \
				  -c -scfw $(IMX8X_SCFW_IMAGE) \
				  -ap $(BUILD_DIR)/pb_hash.bin a35 $(PB_ENTRY) \
				  -out $(BUILD_DIR)/pb.imx
	$(eval FINAL_OUTPUT := $(BUILD_DIR)/$(TARGET).imx)

imx8x_sign_image: imx8x_image
	@cp $(PB_CSF_TEMPLATE) $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__KEY_INDEX__#$(IMX8X_KEY_INDEX)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__KEY_REVOKE_MASK__#$(IMX8X_KEY_REVOKE_MASK)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(IMX8X_SRK_TABLE)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(IMX8X_SIGN_CERT)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__FILE__#$(BUILD_DIR)/pb.imx#g' $(BUILD_DIR)/pb.csf
	@$(CST_TOOL) -i $(BUILD_DIR)/pb.csf -o $(BUILD_DIR)/$(TARGET)_signed.imx  > /dev/null
	$(eval FINAL_OUTPUT := $(BUILD_DIR)/$(TARGET)_signed.imx)

plat-$(CONFIG_IMX8X_CREATE_IMX_IMAGE) += imx8x_image
plat-$(CONFIG_IMX8X_SIGN_IMAGE) += imx8x_sign_image

endif
