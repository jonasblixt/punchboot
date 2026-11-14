
CST_TOOL ?= cst
MKIMAGE ?= mkimage_imx8m

PB_CSF_TEMPLATE = src/plat/imx8m/pb.csf.template
SED = $(shell which sed)

src-y += $(BOARD)/board.c
src-y += $(BOARD)/lpddr4_timing.c

IMX8M_UMCTL_TRAIN_1D_D ?= lpddr4_pmu_train_1d_dmem.bin
IMX8M_UMCTL_TRAIN_1D_I ?= lpddr4_pmu_train_1d_imem.bin
IMX8M_UMCTL_TRAIN_2D_D ?= lpddr4_pmu_train_2d_dmem.bin
IMX8M_UMCTL_TRAIN_2D_I ?= lpddr4_pmu_train_2d_imem.bin

# The signing inputs, from the defconfig unless the command line gives them
IMX8M_SRK_TABLE ?= $(call unquote,$(CONFIG_IMX8M_SRK_TABLE))
IMX8M_KEY_INDEX ?= $(CONFIG_IMX8M_KEY_INDEX)
IMX8M_SIGN_CERT ?= $(call unquote,$(CONFIG_IMX8M_SIGN_CERT))
IMX8M_IMAGE_SIGN_CERT ?= $(call unquote,$(CONFIG_IMX8M_IMAGE_SIGN_CERT))

blobs-y  = $(IMX8M_UMCTL_TRAIN_1D_D) \
		   $(IMX8M_UMCTL_TRAIN_1D_I)\
		   $(IMX8M_UMCTL_TRAIN_2D_D) \
		   $(IMX8M_UMCTL_TRAIN_2D_I)

MKIMAGE_OPTS =
MKIMAGE_OPTS += -loader $(BUILD_DIR)/pb_pad.bin $(CONFIG_PB_ENTRY)
MKIMAGE_OPTS += -out $(BUILD_DIR)/$(TARGET).imx

.PHONY: imx8m_image imx8m_sign_image

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
	$(eval PB_CST_ADDR=0x$(shell echo "obase=16; $$(( $(CONFIG_PB_ENTRY) - 0x40 ))" | bc	))
	@echo "PB imx image size: $(PB_FILESIZE) bytes ($(PB_FILESIZE_HEX)), cst addr $(PB_CST_ADDR)"
	@$(SED) -e 's#__BLOCKS__#Blocks = $(PB_CST_ADDR) 0x0 $(PB_FILESIZE_HEX) "$(BUILD_DIR)/pb_pad.imx"#g' < $(PB_CSF_TEMPLATE) > $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__KEY_INDEX__#$(IMX8M_KEY_INDEX)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(IMX8M_SRK_TABLE)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(IMX8M_SIGN_CERT)#g' $(BUILD_DIR)/pb.csf
	@$(SED) -i -e 's#__IMG_PEM__#$(IMX8M_IMAGE_SIGN_CERT)#g'  $(BUILD_DIR)/pb.csf
	@$(CST_TOOL) -i $(BUILD_DIR)/pb.csf -o $(BUILD_DIR)/pb_csf.bin
	@cp $(BUILD_DIR)/$(TARGET)_pad.imx $(BUILD_DIR)/$(TARGET)_signed.imx
	@echo "Writing signature block at offset $(shell echo "obase=16; $(PB_FILESIZE_IMX2)" | bc	)"
	@dd if=$(BUILD_DIR)/pb_csf.bin of=$(BUILD_DIR)/$(TARGET)_signed.imx seek=$(PB_FILESIZE_IMX2) bs=1 conv=notrunc

image-$(CONFIG_IMX8M_CREATE_IMX_IMAGE) += imx8m_image
image-$(CONFIG_IMX8M_SIGN_IMAGE) += imx8m_sign_image

# What `all` names as the build's output, set here and not by a recipe
ifdef CONFIG_IMX8M_SIGN_IMAGE
FINAL_OUTPUT = $(BUILD_DIR)/$(TARGET)_signed.imx
else ifdef CONFIG_IMX8M_CREATE_IMX_IMAGE
FINAL_OUTPUT = $(BUILD_DIR)/$(TARGET).imx
endif
