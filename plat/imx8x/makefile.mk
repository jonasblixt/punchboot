#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#

ifdef CONFIG_PLAT_IMX8X

CST_TOOL := $(CONFIG_IMX8X_CST_TOOL)
MKIMAGE := $(CONFIG_IMX8X_MKIMAGE_TOOL)

PB_CSF_TEMPLATE = plat/imx8x/pb.csf.template
SED = $(shell which sed)

PLAT_C_SRCS  += plat/imx/usdhc.c
PLAT_C_SRCS  += plat/imx/gpt.c
PLAT_C_SRCS  += plat/imx/lpuart.c
PLAT_C_SRCS  += plat/imx/ehci.c
PLAT_C_SRCS  += plat/imx/caam.c
PLAT_C_SRCS  += plat/imx8x/plat.c
PLAT_C_SRCS  += plat/imx/wdog.c
PLAT_C_SRCS  += plat/imx8x/sci/ipc.c
PLAT_C_SRCS  += plat/imx8x/sci/mx8_mu.c
PLAT_C_SRCS  += plat/imx8x/sci/svc/pad/pad_rpc_clnt.c
PLAT_C_SRCS  += plat/imx8x/sci/svc/pm/pm_rpc_clnt.c
PLAT_C_SRCS  += plat/imx8x/sci/svc/timer/timer_rpc_clnt.c
PLAT_C_SRCS  += plat/imx8x/sci/svc/misc/misc_rpc_clnt.c

PLAT_ASM_SRCS += plat/imx8x/reset_vector.S

CFLAGS += -D__PLAT_IMX8X__
CFLAGS += -I plat/imx8x/include

LDFLAGS += -Tplat/imx8x/link.lds

.PHONY: imx8x_image imx8x_sign_image

imx8x_image: $(BUILD_DIR)/$(TARGET).bin
	@$(MKIMAGE) -commit > $(BUILD_DIR)/head.hash > /dev/null
	@cat $(BUILD_DIR)/pb.bin $(BUILD_DIR)/head.hash > $(BUILD_DIR)/pb_hash.bin
	@$(MKIMAGE) -soc QX -rev B0 \
				  -e emmc_fast \
				  -append $(CONFIG_IMX8X_AHAB_IMAGE) \
				  -c -scfw $(CONFIG_IMX8X_SCFW_IMAGE) \
				  -ap $(BUILD_DIR)/pb_hash.bin a35 $(PB_ENTRY) \
				  -out $(BUILD_DIR)/pb.imx 2> /dev/null > /dev/null
	$(eval FINAL_OUTPUT := $(BUILD_DIR)/$(TARGET).imx)

imx8x_sign_image: imx8x_image
	@cp $(PB_CSF_TEMPLATE) $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(CONFIG_IMX8X_SRK_TABLE)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(CONFIG_IMX8X_SIGN_CERT)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__FILE__#$(BUILD_DIR)/pb.imx#g' $(BUILD_DIR)/pb.csf
	@$(CST_TOOL) -i $(BUILD_DIR)/pb.csf -o $(BUILD_DIR)/$(TARGET)_signed.imx  > /dev/null
	$(eval FINAL_OUTPUT := $(BUILD_DIR)/$(TARGET)_signed.imx)

plat-$(CONFIG_IMX8X_CREATE_IMX_IMAGE) += imx8x_image
plat-$(CONFIG_IMX8X_SIGN_IMAGE) += imx8x_sign_image

endif
