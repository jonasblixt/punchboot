#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv8a

CST_TOOL ?= ~/work/cst-3.1.0/linux64/bin/cst

SRK_TBL  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_table.bin)
CSFK_PEM ?= $(shell realpath ../pki/imx6ul_hab_testkeys/CSF1_1_sha256_4096_65537_v3_usr_crt.pem)
IMG_PEM  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/IMG1_1_sha256_4096_65537_v3_usr_crt.pem)
SRK_FUSE_BIN ?= $(shell realpath ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_fuse.bin)

PB_CSF_TEMPLATE = plat/imx8x/pb.csf.template
SED = $(shell which sed)

PLAT_C_SRCS  += plat/imx/usdhc.c
PLAT_C_SRCS  += plat/imx/gpt.c
PLAT_C_SRCS  += plat/imx/lpuart.c
PLAT_C_SRCS  += plat/imx/ehci.c
PLAT_C_SRCS  += plat/imx/caam.c
PLAT_C_SRCS  += plat/imx8x/plat.c
PLAT_C_SRCS  += plat/imx/wdog.c
PLAT_C_SRCS  += plat/imx8x/sci/ipc.c
PLAT_C_SRCS  += plat/imx8x/sci/mx8_mu.c
PLAT_C_SRCS  += plat/imx8x/sci/svc/pad/pad_rpc_clnt.c
PLAT_C_SRCS  += plat/imx8x/sci/svc/pm/pm_rpc_clnt.c

CFLAGS += -I plat/imx8x/include

$(eval PB_SRKS=$(shell hexdump -e '/4 "0x"' -e '/4 "%X"",\n"' < $(SRK_FUSE_BIN)))

$(shell echo "#include <stdint.h>" > plat/imx8x/hab_srks.c)
$(shell echo "const uint32_t build_root_hash[8] ={$(PB_SRKS)};" >> plat/imx8x/hab_srks.c)
PLAT_C_SRCS  += plat/imx8x/hab_srks.c

plat_clean:
	@-rm -rf plat/imx8x/*.o
	@-rm -rf plat/imx8x/hab_srks.*

plat_final:
	@mkimage_imx8 -commit > head.hash
	@cat pb.bin head.hash > pb_hash.bin
	@mkimage_imx8 -soc QX -rev B0 \
				  -append /work/firmware-imx-8.0/firmware/seco/mx8qx-ahab-container.img \
				  -c -scfw /work/acu6c-boot/acu6c-scfw/build_mx8qx_b0/scfw_tcm.bin \
				  -ap pb_hash.bin a35 0x80000000 -out pb.imx

