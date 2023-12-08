PB_ENTRY      = 0x20249000
MKIMAGE ?= mkimage

src-y += $(BOARD)/board.c

ldflags-y += -T$(BOARD)/link.lds

imxrt_image: $(BUILD_DIR)/$(TARGET).bin
	@$(MKIMAGE) -n $(BOARD)/imximage.cfg -T imximage -e $(PB_ENTRY) \
				-d $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).imx

	$(eval FINAL_OUTPUT := $(BUILD_DIR)/$(TARGET).imx)

plat-y += imxrt_image
