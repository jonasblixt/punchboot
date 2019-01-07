#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv8a

CST_TOOL ?= /work/cst_2.3.3

SRK_TBL  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_table.bin)
CSFK_PEM ?= $(shell realpath ../pki/imx6ul_hab_testkeys/CSF1_1_sha256_4096_65537_v3_usr_crt.pem)
IMG_PEM  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/IMG1_1_sha256_4096_65537_v3_usr_crt.pem)
SRK_FUSE_BIN ?= $(shell realpath ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_fuse.bin)

PB_CSF_TEMPLATE = plat/imx8m/pb.csf.template
SED = $(shell which sed)

PLAT_C_SRCS  += plat/imx6ul/imx_uart.c

$(eval PB_SRKS=$(shell hexdump -e '/4 "0x"' -e '/4 "%X"",\n"' < $(SRK_FUSE_BIN)))
$(shell rm -f plat/imx6ul/hab_srks.*)
$(shell echo "#include <stdint.h>\nconst uint32_t build_root_hash[8] ={$(PB_SRKS)};" > plat/imx6ul/hab_srks.c)
# PLAT_C_SRCS  += plat/imx8m/hab_srks.c

plat_clean:
	@-rm -rf plat/imx8m/*.o
	@-rm -rf plat/imx8m/hab_srks.*

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
