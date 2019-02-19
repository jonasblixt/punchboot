#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv8a

CST_TOOL ?= /work/cst-3.1.0/linux64/bin/cst

SRK_TBL  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_table.bin)
CSFK_PEM ?= $(shell realpath ../pki/imx6ul_hab_testkeys/CSF1_1_sha256_4096_65537_v3_usr_crt.pem)
IMG_PEM  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/IMG1_1_sha256_4096_65537_v3_usr_crt.pem)
SRK_FUSE_BIN ?= $(shell realpath ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_fuse.bin)

PB_CSF_TEMPLATE = plat/imx8m/pb.csf.template
SED = $(shell which sed)

PLAT_C_SRCS  += plat/imx/imx_uart.c
PLAT_C_SRCS  += plat/imx/usdhc.c
PLAT_C_SRCS  += plat/imx/gpt.c
PLAT_C_SRCS  += plat/imx/caam.c
PLAT_C_SRCS  += plat/imx/dwc3.c
PLAT_C_SRCS  += plat/imx/hab.c
PLAT_C_SRCS  += plat/imx/ocotp.c
PLAT_C_SRCS  += plat/imx8m/plat.c
PLAT_C_SRCS  += plat/imx/wdog.c

BLOB_INPUT += lpddr4_pmu_train_1d_dmem.bin lpddr4_pmu_train_2d_dmem.bin
BLOB_INPUT += lpddr4_pmu_train_1d_imem.bin lpddr4_pmu_train_2d_imem.bin

CFLAGS += -I plat/imx8m/include

plat_clean:
	@-rm -rf plat/imx8m/*.o

plat_final:
	@echo "plat_final"
	$(eval PB_FILESIZE=$(shell stat -c%s "pb.imx"))
	$(eval PB_FILESIZE2=$(shell echo " $$(( $(PB_FILESIZE) - 0x2000 ))" | bc	))
	$(eval PB_FILESIZE_HEX=0x$(shell echo "obase=16; $(PB_FILESIZE2)" | bc	))
	$(eval PB_CST_ADDR=0x$(shell echo "obase=16; $$(( $(PB_ENTRY) - 0x30 ))" | bc	))
	@echo "PB imx image size: $(PB_FILESIZE) bytes ($(PB_FILESIZE_HEX)), cst addr $(PB_CST_ADDR)"
	@$(SED) -e 's/__BLOCKS__/Blocks = $(PB_CST_ADDR) 0x000 $(PB_FILESIZE_HEX) "pb.imx"/g' < $(PB_CSF_TEMPLATE) > pb.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(SRK_TBL)#g' pb.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(CSFK_PEM)#g' pb.csf
	@$(SED) -i -e 's#__IMG_PEM__#$(IMG_PEM)#g'  pb.csf
	@$(CST_TOOL) --o pb_csf.bin --i pb.csf

