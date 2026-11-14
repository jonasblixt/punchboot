MKIMAGE ?= mkimage

src-y += $(BOARD)/board.c

# The FlexSPI configuration block for the revision's QSPI NOR (Kconfig)
fcb-$(CONFIG_BOARD_IMXRT1060EVK_REV_A) = $(BOARD)/fcb_is25wp064.c
fcb-$(CONFIG_BOARD_IMXRT1060EVK_REV_B) = $(BOARD)/fcb_is25wp064.c
fcb-$(CONFIG_BOARD_IMXRT1060EVK_REV_C) = $(BOARD)/fcb_w25q128jw.c

imxrt_image: $(BUILD_DIR)/$(TARGET).bin
	@$(MKIMAGE) -n $(BOARD)/imximage.cfg -T imximage -e $(CONFIG_PB_ENTRY) \
				-d $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).imx

$(BUILD_DIR)/fcb.bin: $(fcb-y)
	@echo FCB $<
	$(Q)$(CC) -c $(cflags-y) $< -o $(BUILD_DIR)/fcb.o
	$(Q)$(OBJCOPY) -O binary -j .fcb $(BUILD_DIR)/fcb.o $@
	$(Q)test $$(stat -c %s $@) -eq 512

# What goes in the QSPI NOR from offset 0: the FCB, zeros up to the IVT at
# 0x1000 (RM 9.7.1), then pb.imx. The boot ROM copies it to the OCRAM.
imxrt_flash_image: imxrt_image $(BUILD_DIR)/fcb.bin
	$(Q)cp $(BUILD_DIR)/fcb.bin $(BUILD_DIR)/$(TARGET)_flash.bin
	$(Q)truncate -s 4096 $(BUILD_DIR)/$(TARGET)_flash.bin
	$(Q)cat $(BUILD_DIR)/$(TARGET).imx >> $(BUILD_DIR)/$(TARGET)_flash.bin

image-$(CONFIG_IMXRT_CREATE_IMX_IMAGE) += imxrt_image
image-$(CONFIG_IMXRT_CREATE_FLASH_IMAGE) += imxrt_flash_image

# What `all` names as the build's output, set here and not by a recipe
ifdef CONFIG_IMXRT_CREATE_FLASH_IMAGE
FINAL_OUTPUT = $(BUILD_DIR)/$(TARGET)_flash.bin
else ifdef CONFIG_IMXRT_CREATE_IMX_IMAGE
FINAL_OUTPUT = $(BUILD_DIR)/$(TARGET).imx
endif
