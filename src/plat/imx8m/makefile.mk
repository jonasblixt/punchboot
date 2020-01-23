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

SPL_C_SRCS   = plat/imx8m/spl.c
SPL_C_SRCS  += plat/imx8m/umctl2_lp4.c

SPL_LDFLAGS += --defsym=PB_ENTRY=$(PB_ENTRY)
SPL_LDFLAGS += -Tarch/$(PB_ARCH_NAME)/link.lds
SPL_LDFLAGS += -Tplat/imx8m/spl.lds
SPL_LDFLAGS += --build-id=none

SPL_BLOB_INPUT  = lpddr4_pmu_train_1d_dmem.bin
SPL_BLOB_INPUT += lpddr4_pmu_train_2d_dmem.bin
SPL_BLOB_INPUT += lpddr4_pmu_train_1d_imem.bin
SPL_BLOB_INPUT += lpddr4_pmu_train_2d_imem.bin

CFLAGS += -I plat/imx8m/include

SPL_BLOB_OBJS = $(patsubst %.bin, $(BUILD_DIR)/%.bino, $(SPL_BLOB_INPUT))

.PHONY: imx8m_spl

$(BUILD_DIR)/spl.bin: $(BUILD_DIR)/spl
	@echo OBJCOPY $< $@
	@$(STRIP) --strip-all $<
	@$(OBJCOPY) -O binary -R .comment $< $@

$(BUILD_DIR)/spl: $(SPL_OBJS) $(SPL_BLOB_OBJS)
	@echo LD $@
	@$(LD) $(SPL_LDFLAGS) $(SPL_OBJS) $(SPL_BLOB_OBJS) -o $@

imx8m_spl: $(BUILD_DIR)/spl.bin
	@echo Building imx8m SPL

imx8m_image:
	@echo Generating $(BUILD_DIR)/$(TARGET).imx
	@$(MKIMAGE) -loader $(BUILD_DIR)/spl.bin $(PB_ENTRY) \
		-second_loader $(BUILD_DIR)/$(TARGET).bin 0x40020000 0x60000 \
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

