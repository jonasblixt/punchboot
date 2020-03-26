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
MKIMAGE ?= mkimage_imx8_imx8m

SRK_TBL  ?= $(shell readlink -f ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_table.bin)
CSFK_PEM ?= $(shell readlink -f ../pki/imx6ul_hab_testkeys/CSF1_1_sha256_4096_65537_v3_usr_crt.pem)
IMG_PEM  ?= $(shell readlink -f ../pki/imx6ul_hab_testkeys/IMG1_1_sha256_4096_65537_v3_usr_crt.pem)
SRK_FUSE_BIN ?= $(shell readlink -f ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_fuse.bin)

PB_CSF_TEMPLATE = plat/imx8m/pb.csf.template
SED = $(shell which sed)

PLAT_C_SRCS  += plat/imx8m/plat.c
PLAT_C_SRCS  += plat/imx/imx_uart.c
PLAT_C_SRCS  += plat/imx/usdhc.c
PLAT_C_SRCS  += plat/imx/gpt.c
PLAT_C_SRCS  += plat/imx/caam.c
PLAT_C_SRCS  += plat/imx/dwc3.c
PLAT_C_SRCS  += plat/imx/hab.c
PLAT_C_SRCS  += plat/imx/ocotp.c
PLAT_C_SRCS  += plat/imx/wdog.c
PLAT_C_SRCS  += plat/imx8m/umctl2_lp4.c

BLOB_INPUT  = lpddr4_pmu_train_1d_dmem.bin
BLOB_INPUT += lpddr4_pmu_train_2d_dmem.bin
BLOB_INPUT += lpddr4_pmu_train_1d_imem.bin
BLOB_INPUT += lpddr4_pmu_train_2d_imem.bin

CFLAGS += -I plat/imx8m/include

.PHONY: imx8m_spl

imx8m_image: $(BUILD_DIR)/pb.bin
	@echo Generating $(BUILD_DIR)/$(TARGET).imx
	@$(MKIMAGE) -loader $(BUILD_DIR)/pb.bin $(PB_ENTRY) \
		-out $(BUILD_DIR)/$(TARGET).imx

plat_final: imx8m_spl imx8m_image
	$(eval PB_FILESIZE=$(shell stat -c%s "${BUILD_DIR}/pb.imx"))
	@echo $(PB_FILESIZE)
	$(eval PB_FILESIZE2=$(shell echo " $$(( $(PB_FILESIZE) - 0x2000 ))" | bc	))
	$(eval PB_FILESIZE_HEX=0x$(shell echo "obase=16; $(PB_FILESIZE2)" | bc	))
	$(eval PB_CST_ADDR=0x$(shell echo "obase=16; $$(( $(PB_ENTRY) - 0x30 ))" | bc	))
	@echo "PB imx image size: $(PB_FILESIZE) bytes ($(PB_FILESIZE_HEX)), cst addr $(PB_CST_ADDR)"
	@$(SED) -e 's/__BLOCKS__/Blocks = $(PB_CST_ADDR) 0x000 $(PB_FILESIZE_HEX) "$(BUILD_DIR)\/pb.imx"/g' < $(PB_CSF_TEMPLATE) > $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(SRK_TBL)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(CSFK_PEM)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__IMG_PEM__#$(IMG_PEM)#g'  $(BUILD_DIR)/pb.csf
	@$(CST_TOOL) --o $(BUILD_DIR)/pb_csf.bin --i $(BUILD_DIR)/pb.csf
	@cp $(BUILD_DIR)/$(TARGET).imx $(BUILD_DIR)/$(TARGET)_signed.imx
	@dd if=$(BUILD_DIR)/pb_csf.bin of=$(BUILD_DIR)/$(TARGET)_signed.imx seek=$(PB_FILESIZE2) bs=1 conv=notrunc

