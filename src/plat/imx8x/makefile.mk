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
MKIMAGE ?= $(shell which mkimage_imx8)


SRK_TBL  ?= ../pki/imx8x_ahab/crts/SRK_1_2_3_4_table.bin
CSFK_PEM ?= ../pki/imx8x_ahab/crts/SRK1_sha384_secp384r1_v3_usr_crt.pem
SRK_FUSE_BIN ?= ../pki/imx8x_ahab/crts/SRK_1_2_3_4_fuse.bin
AHAB_CONTAINER ?= mx8qx-ahab-container.img
SCFW_BIN ?= scfw_tcm.bin

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
PLAT_C_SRCS  += plat/imx8x/sci/svc/timer/timer_rpc_clnt.c
PLAT_C_SRCS  += plat/imx8x/sci/svc/misc/misc_rpc_clnt.c

PLAT_ASM_SRCS += plat/imx8x/reset_vector.S

CFLAGS += -D__PLAT_IMX8X__
CFLAGS += -I plat/imx8x/include

plat_clean:
	@-rm -rf plat/imx8x/*.o
	@-rm -rf plat/imx8x/hab_srks.*

plat_final:
	@$(MKIMAGE) -commit > head.hash
	@cat pb.bin head.hash > pb_hash.bin
	@$(MKIMAGE) -soc QX -rev B0 \
				  -e emmc_fast \
				  -append $(AHAB_CONTAINER) \
				  -c -scfw $(SCFW_BIN) \
				  -ap pb_hash.bin a35 0x80000000 \
				  -out pb.imx
	@cp $(PB_CSF_TEMPLATE) pb.csf
	@$(SED) -i -e 's#__SRK_TBL__#$(SRK_TBL)#g' pb.csf
	@$(SED) -i -e 's#__CSFK_PEM__#$(CSFK_PEM)#g' pb.csf
	@$(CST_TOOL) -i pb.csf -o $(TARGET)_signed.imx 
