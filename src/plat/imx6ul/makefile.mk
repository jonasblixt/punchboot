#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv7a

CST_TOOL ?= tools/imxcst/src/build-x86_64-linux-gnu/cst
MKIMAGE ?= mkimage

SRK_TBL  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_table.bin)
CSFK_PEM ?= $(shell realpath ../pki/imx6ul_hab_testkeys/CSF1_1_sha256_4096_65537_v3_usr_crt.pem)
IMG_PEM  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/IMG1_1_sha256_4096_65537_v3_usr_crt.pem)
SRK_FUSE_BIN ?= $(shell realpath ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_fuse.bin)

PB_CSF_TEMPLATE = plat/imx6ul/pb.csf.template
PB_UUU_CSF_TEMPLATE = plat/imx6ul/pb_uuu.csf.template

SED = $(shell which sed)

CFLAGS += -I plat/imx6ul/include

PLAT_C_SRCS  += plat/imx/imx_uart.c
PLAT_C_SRCS  += plat/imx6ul/plat.c
PLAT_C_SRCS  += plat/imx/ehci.c
PLAT_C_SRCS  += plat/imx/usdhc.c
PLAT_C_SRCS  += plat/imx/gpt.c
PLAT_C_SRCS  += plat/imx/caam.c
PLAT_C_SRCS  += plat/imx/ocotp.c
PLAT_C_SRCS	 += plat/imx/wdog.c
PLAT_C_SRCS  += plat/imx/hab.c

plat_clean:
	@-rm -rf plat/imx/*.o
	@-rm -rf plat/imx6ul/*.o

imx6ul_image:
	@$(MKIMAGE) -n board/$(BOARD)/imximage.cfg -T imximage -e $(PB_ENTRY) \
				-d $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).imx 

plat_final: imx6ul_image
	@echo Using $(CST_TOOL)
	$(eval PB_FILESIZE=$(shell stat -c%s "${BUILD_DIR}/pb.imx"))
	$(eval PB_FILESIZE_HEX=0x$(shell echo "obase=16; $(PB_FILESIZE)" | bc	))
	$(eval PB_CST_ADDR=0x$(shell echo "obase=16; $$(( $(PB_ENTRY) - 0xC00 ))" | bc	))
	@echo "PB imx image size: $(PB_FILESIZE) bytes ($(PB_FILESIZE_HEX)), cst addr $(PB_CST_ADDR)"
	@$(SED) -e 's/__BLOCKS__/Blocks = $(PB_CST_ADDR) 0x000 $(PB_FILESIZE_HEX) "$(BUILD_DIR)\/pb.imx"/g' < $(PB_CSF_TEMPLATE) > $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(SRK_TBL)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(CSFK_PEM)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__IMG_PEM__#$(IMG_PEM)#g'  $(BUILD_DIR)/pb.csf
	@$(CST_TOOL) --o $(BUILD_DIR)/pb_csf.bin --i $(BUILD_DIR)/pb.csf
	@cat $(BUILD_DIR)/pb.imx $(BUILD_DIR)/pb_csf.bin > $(BUILD_DIR)/pb_signed.imx
	@echo Done
	$(eval PB_OFFSET=0x$(shell dd if=$(BUILD_DIR)/pb.imx bs=1 skip=45 count=2 2>/dev/null | xxd -p))
	@$(SED) -e 's/__UUU_BLOCKS__/Blocks = 0x00910000 0x02c $(PB_OFFSET) "$(BUILD_DIR)\/pb.imx"/g' < $(PB_UUU_CSF_TEMPLATE) > $(BUILD_DIR)/pb_uuu.csf
	@$(SED) -i -e 's/__BLOCKS__/Blocks = $(PB_CST_ADDR) 0x000 $(PB_FILESIZE_HEX) "$(BUILD_DIR)\/pb.imx"/g' $(BUILD_DIR)/pb_uuu.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(SRK_TBL)#g' $(BUILD_DIR)/pb_uuu.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(CSFK_PEM)#g' $(BUILD_DIR)/pb_uuu.csf
	@$(SED) -i -e 's#__IMG_PEM__#$(IMG_PEM)#g'  $(BUILD_DIR)/pb_uuu.csf
	@$(CST_TOOL) --o $(BUILD_DIR)/pb_csf_uuu.bin --i $(BUILD_DIR)/pb_uuu.csf
	@cat $(BUILD_DIR)/pb.imx $(BUILD_DIR)/pb_csf_uuu.bin > $(BUILD_DIR)/pb_signed_uuu.imx
