#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv7a

CST_TOOL ?= /work/cst_2.3.3

SRK_TBL  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_table.bin)
CSFK_PEM ?= $(shell realpath ../pki/imx6ul_hab_testkeys/CSF1_1_sha256_4096_65537_v3_usr_crt.pem)
IMG_PEM  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/IMG1_1_sha256_4096_65537_v3_usr_crt.pem)
SRK_FUSE_BIN ?= $(shell realpath ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_fuse.bin)

PB_CSF_TEMPLATE = plat/imx6ul/pb.csf.template
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

plat_final:
	$(eval PB_FILESIZE=$(shell stat -c%s "pb.imx"))
	$(eval PB_FILESIZE_HEX=0x$(shell echo "obase=16; $(PB_FILESIZE)" | bc	))
	$(eval PB_CST_ADDR=0x$(shell echo "obase=16; $$(( $(PB_ENTRY) - 0xC00 ))" | bc	))
	@echo "PB imx image size: $(PB_FILESIZE) bytes ($(PB_FILESIZE_HEX)), cst addr $(PB_CST_ADDR)"
	@$(SED) -e 's/__BLOCKS__/Blocks = $(PB_CST_ADDR) 0x000 $(PB_FILESIZE_HEX) "pb.imx"/g' < $(PB_CSF_TEMPLATE) > pb.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(SRK_TBL)#g' pb.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(CSFK_PEM)#g' pb.csf
	@$(SED) -i -e 's#__IMG_PEM__#$(IMG_PEM)#g'  pb.csf
	@$(CST_TOOL) --o pb_csf.bin --i pb.csf
	@cat pb.imx pb_csf.bin > pb_signed.imx
