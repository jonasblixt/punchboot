PB_ENTRY     = 0x80000000
CST_TOOL ?= cst
MKIMAGE ?= mkimage_imx8dxl
IMX8XL_AHAB_IMAGE ?= mx8dxlb0-ahab-container.img
IMX8XL_SCFW_IMAGE ?= mx8dxl-evk-scfw-tcm.bin
IMX8XL_SRK_TABLE ?= pki/imx8x_ahab/crts/SRK_1_2_3_4_table.bin
IMX8XL_SIGN_CERT ?= pki/imx8x_ahab/crts/SRK1_sha384_secp384r1_v3_usr_crt.pem
IMX8XL_KEY_INDEX ?= 0
IMX8XL_KEY_REVOKE_MASK ?= 0
PB_CSF_TEMPLATE = src/plat/imx8x/pb.csf.template

SED = $(shell which sed)

src-y += $(BOARD)/board.c

cflags-y += -DBOARD_RAM_BASE=0x80000000
cflags-y += -DBOARD_RAM_END=0x140000000

.PHONY: imx8xl_image imx8xl_sign_image

imx8xl_image: $(BUILD_DIR)/$(TARGET).bin
	@echo Using SCFW image: $(shell readlink -f $(IMX8XL_SCFW_IMAGE))
	@echo Using AHAB image: $(shell readlink -f $(IMX8XL_AHAB_IMAGE))
	@echo Using SRK Table: $(shell readlink -f $(IMX8XL_SRK_TABLE))
	@echo Using signing cert: $(shell readlink -f $(IMX8XL_SIGN_CERT))
	@$(MKIMAGE) -commit > $(BUILD_DIR)/head.hash
	@cat $(BUILD_DIR)/pb.bin $(BUILD_DIR)/head.hash > $(BUILD_DIR)/pb_hash.bin
	$(Q)$(MKIMAGE) -soc DXL -rev B0 \
				  -e emmc_fast \
				  -append $(IMX8XL_AHAB_IMAGE) \
				  -c -scfw $(IMX8XL_SCFW_IMAGE) \
				  -ap $(BUILD_DIR)/pb_hash.bin a35 $(PB_ENTRY) \
				  -out $(BUILD_DIR)/pb.imx
	$(eval FINAL_OUTPUT := $(BUILD_DIR)/$(TARGET).imx)

imx8xl_sign_image: imx8xl_image
	@echo Signing not yet supported on imx8dxl
	@exit 1

plat-$(CONFIG_IMX8X_CREATE_IMX_IMAGE) += imx8xl_image
plat-$(CONFIG_IMX8X_SIGN_IMAGE) += imx8xl_sign_image
