#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_IMX8M

CST_TOOL ?= cst
MKIMAGE ?= mkimage_imx8m

PB_CSF_TEMPLATE = plat/imx8m/pb.csf.template
SED = $(shell which sed)

src-y  += plat/imx8m/plat.c
src-y  += plat/imx/imx_uart.c
src-y  += plat/imx/usdhc.c
src-y  += plat/imx/caam.c
src-y  += plat/imx/dwc3.c
src-y  += plat/imx/hab.c
src-y  += plat/imx/ocotp.c
src-y  += plat/imx/wdog.c
src-y  += plat/imx8m/umctl2_lp4.c

blobs-y  = $(patsubst "%",%,$(CONFIG_IMX8M_UMCTL_TRAIN_1D_D)) \
	$(patsubst "%",%,$(CONFIG_IMX8M_UMCTL_TRAIN_1D_I)) \
	$(patsubst "%",%,$(CONFIG_IMX8M_UMCTL_TRAIN_2D_D)) \
	$(patsubst "%",%,$(CONFIG_IMX8M_UMCTL_TRAIN_2D_I))

cflags-y += -I plat/imx8m/include
ldflags-y += -Tplat/imx8m/link.lds

MKIMAGE_OPTS =
#MKIMAGE_OPTS += -dev emmc_fastboot 
#MKIMAGE_OPTS += -version v1
#MKIMAGE_OPTS += -fit
MKIMAGE_OPTS += -signed_hdmi blobs/signed_hdmi_imx8m.bin
MKIMAGE_OPTS += -loader $(BUILD_DIR)/pb_pad.bin $(PB_ENTRY)
#MKIMAGE_OPTS += -second_loader $(BUILD_DIR)/arne 0x48000000 0x20000
MKIMAGE_OPTS += -out $(BUILD_DIR)/$(TARGET).imx

.PHONY: imx8m_image imx8m_sign_image

# -signed_hdmi blobs/signed_hdmi_imx8m.bin
imx8m_image: $(BUILD_DIR)/pb.bin
	@echo Generating $(BUILD_DIR)/$(TARGET).imx
	@dd if=$(BUILD_DIR)/$(TARGET).bin of=$(BUILD_DIR)/$(TARGET)_pad.bin bs=512 conv=sync,noerror
	@$(MKIMAGE) $(MKIMAGE_OPTS)
	@dd if=$(BUILD_DIR)/$(TARGET).imx of=$(BUILD_DIR)/$(TARGET)_pad.imx bs=4 conv=sync,noerror
	@echo "$(BUILD_DIR)/$(TARGET)_pad.imx ready"

imx8m_sign_image: imx8m_image
	$(eval PB_FILESIZE=$(shell stat -c%s "${BUILD_DIR}/pb_pad.bin"))
	$(eval PB_FILESIZE_IMX=$(shell stat -c%s "${BUILD_DIR}/pb_pad.imx"))
	$(eval PB_FILESIZE_IMX2=$(shell echo " $$(( $(PB_FILESIZE_IMX) - 0x2000 ))" | bc	))
	$(eval PB_FILESIZE2=$(shell echo " $$(( $(PB_FILESIZE) + 0x200 ))" | bc	))
	$(eval PB_FILESIZE_HEX=0x$(shell echo "obase=16; $(PB_FILESIZE2)" | bc	))
	$(eval PB_CST_ADDR=0x$(shell echo "obase=16; $$(( $(PB_ENTRY) - 0x40 ))" | bc	))
	@echo "PB imx image size: $(PB_FILESIZE) bytes ($(PB_FILESIZE_HEX)), cst addr $(PB_CST_ADDR)"
	@$(SED) -e 's/__BLOCKS__/Blocks = $(PB_CST_ADDR) 0x1a000 $(PB_FILESIZE_HEX) "$(BUILD_DIR)\/pb_pad.imx"/g' < $(PB_CSF_TEMPLATE) > $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__KEY_INDEX__#$(CONFIG_IMX8M_KEY_INDEX)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(CONFIG_IMX8M_SRK_TABLE)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(CONFIG_IMX8M_SIGN_CERT)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__IMG_PEM__#$(CONFIG_IMX8M_IMAGE_SIGN_CERT)#g'  $(BUILD_DIR)/pb.csf
	@$(CST_TOOL) -i $(BUILD_DIR)/pb.csf -o $(BUILD_DIR)/pb_csf.bin
	@cp $(BUILD_DIR)/$(TARGET)_pad.imx $(BUILD_DIR)/$(TARGET)_signed.imx
	@echo "Writing signature block at offset $(shell echo "obase=16; $(PB_FILESIZE_IMX2)" | bc	)"
	@dd if=$(BUILD_DIR)/pb_csf.bin of=$(BUILD_DIR)/$(TARGET)_signed.imx seek=$(PB_FILESIZE_IMX2) bs=1 conv=notrunc

plat-$(CONFIG_IMX8M_CREATE_IMX_IMAGE) += imx8m_image
plat-$(CONFIG_IMX8M_SIGN_IMAGE) += imx8m_sign_image
endif
