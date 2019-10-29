#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv8a

CST_TOOL ?= tools/imxcst/src/build-x86_64-linux-gnu/cst
MKIMAGE ?= $(shell which mkimage_imx8)


SRK_TBL  ?= ../pki/imx8x_ahab/crts/SRK_1_2_3_4_table.bin
CSFK_PEM ?= ../pki/imx8x_ahab/crts/SRK1_sha384_secp384r1_v3_usr_crt.pem
SRK_FUSE_BIN ?= ../pki/imx8x_ahab/crts/SRK_1_2_3_4_fuse.bin
AHAB_CONTAINER ?= mx8qx-ahab-container.img
SCFW_BIN ?= scfw_tcm.bin

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

imx8x_image:
	@$(MKIMAGE) -commit > $(BUILD_DIR)/head.hash
	@cat $(BUILD_DIR)/pb.bin $(BUILD_DIR)/head.hash > $(BUILD_DIR)/pb_hash.bin
	@$(MKIMAGE) -soc QX -rev B0 \
				  -e emmc_fast \
				  -append $(AHAB_CONTAINER) \
				  -c -scfw $(SCFW_BIN) \
				  -ap $(BUILD_DIR)/pb_hash.bin a35 $(PB_ENTRY) \
				  -out $(BUILD_DIR)/pb.imx

plat_final: imx8x_image
	@cp $(PB_CSF_TEMPLATE) $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(SRK_TBL)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(CSFK_PEM)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__FILE__#$(BUILD_DIR)/pb.imx#g' $(BUILD_DIR)/pb.csf
	@$(CST_TOOL) -i $(BUILD_DIR)/pb.csf -o $(BUILD_DIR)/$(TARGET)_signed.imx 
