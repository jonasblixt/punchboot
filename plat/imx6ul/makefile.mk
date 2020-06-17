#
# Punch BOOT
#
# Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_IMX6UL

CST_TOOL ?= cst
MKIMAGE ?= mkimage

PB_CSF_TEMPLATE = plat/imx6ul/pb.csf.template
PB_UUU_CSF_TEMPLATE = plat/imx6ul/pb_uuu.csf.template

SED = $(shell which sed)

CFLAGS += -I plat/imx6ul/include

LDFLAGS += -Tplat/imx6ul/link.lds

PLAT_C_SRCS  += plat/imx/imx_uart.c
PLAT_C_SRCS  += plat/imx6ul/plat.c
PLAT_C_SRCS  += plat/imx/ehci.c
PLAT_C_SRCS  += plat/imx/usdhc.c
PLAT_C_SRCS  += plat/imx/caam.c
PLAT_C_SRCS  += plat/imx/ocotp.c
PLAT_C_SRCS	 += plat/imx/wdog.c
PLAT_C_SRCS  += plat/imx/hab.c

imx6ul_image: $(BUILD_DIR)/$(TARGET).bin
	@$(MKIMAGE) -n board/jiffy/imximage.cfg -T imximage -e $(PB_ENTRY) \
				-d $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).imx 

	$(eval FINAL_OUTPUT := $(BUILD_DIR)/$(TARGET).imx)

imx6ul_sign_image: imx6ul_image
	@echo Using $(CST_TOOL)
	$(eval PB_FILESIZE=$(shell stat -c%s "${BUILD_DIR}/pb.imx"))
	$(eval PB_FILESIZE_HEX=0x$(shell echo "obase=16; $(PB_FILESIZE)" | bc	))
	$(eval PB_CST_ADDR=0x$(shell echo "obase=16; $$(( $(PB_ENTRY) - 0xC00 ))" | bc	))
	@echo "PB imx image size: $(PB_FILESIZE) bytes ($(PB_FILESIZE_HEX)), cst addr $(PB_CST_ADDR)"
	@$(SED) -e 's/__BLOCKS__/Blocks = $(PB_CST_ADDR) 0x000 $(PB_FILESIZE_HEX) "$(BUILD_DIR)\/pb.imx"/g' < $(PB_CSF_TEMPLATE) > $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__KEY_INDEX__#$(CONFIG_IMX6UL_KEY_INDEX)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(CONFIG_IMX6UL_SRK_TABLE)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(CONFIG_IMX6UL_SIGN_CERT)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__IMG_PEM__#$(CONFIG_IMX6UL_IMAGE_SIGN_CERT)#g'  $(BUILD_DIR)/pb.csf
	@$(CST_TOOL) --o $(BUILD_DIR)/pb_csf.bin --i $(BUILD_DIR)/pb.csf
	@cat $(BUILD_DIR)/pb.imx $(BUILD_DIR)/pb_csf.bin > $(BUILD_DIR)/pb_signed.imx
	@echo Done
	$(eval PB_OFFSET=0x$(shell dd if=$(BUILD_DIR)/pb.imx bs=1 skip=45 count=2 2>/dev/null | xxd -p))
	@$(SED) -e 's/__UUU_BLOCKS__/Blocks = 0x00910000 0x02c $(PB_OFFSET) "$(BUILD_DIR)\/pb.imx"/g' < $(PB_UUU_CSF_TEMPLATE) > $(BUILD_DIR)/pb_uuu.csf
	@$(SED) -i -e 's/__BLOCKS__/Blocks = $(PB_CST_ADDR) 0x000 $(PB_FILESIZE_HEX) "$(BUILD_DIR)\/pb.imx"/g' $(BUILD_DIR)/pb_uuu.csf
	@$(SED) -i -e 's#__KEY_INDEX__#$(CONFIG_IMX6UL_KEY_INDEX)#g' $(BUILD_DIR)/pb_uuu.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(CONFIG_IMX6UL_SRK_TABLE)#g' $(BUILD_DIR)/pb_uuu.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(CONFIG_IMX6UL_SIGN_CERT)#g' $(BUILD_DIR)/pb_uuu.csf
	@$(SED) -i -e 's#__IMG_PEM__#$(CONFIG_IMX6UL_IMAGE_SIGN_CERT)#g'  $(BUILD_DIR)/pb_uuu.csf
	@$(CST_TOOL) --o $(BUILD_DIR)/pb_csf_uuu.bin --i $(BUILD_DIR)/pb_uuu.csf
	@cat $(BUILD_DIR)/pb.imx $(BUILD_DIR)/pb_csf_uuu.bin > $(BUILD_DIR)/pb_signed_uuu.imx

	$(eval FINAL_OUTPUT := $(BUILD_DIR)/$(TARGET)_signed.imx)

plat-$(CONFIG_IMX6UL_CREATE_IMX_IMAGE) += imx6ul_image
plat-$(CONFIG_IMX6UL_SIGN_IMAGE) += imx6ul_sign_image

endif
