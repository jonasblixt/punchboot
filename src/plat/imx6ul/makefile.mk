#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv7a

SED = $(shell which sed)
CSF_SIGN_TOOL ?= tools/csftool

PB_SRK_TABLE  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/SRK_1_2_3_4_table.bin)
PB_CSF_KEY ?= $(shell realpath ../pki/imx6ul_hab_testkeys/CSF1_1_sha256_4096_65537_v3_usr_key.pem)
PB_CSF_CRT ?= $(shell realpath ../pki/imx6ul_hab_testkeys/CSF1_1_sha256_4096_65537_v3_usr_crt.pem)
PB_IMG_KEY  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/IMG1_1_sha256_4096_65537_v3_usr_key.pem)
PB_IMG_CRT  ?= $(shell realpath ../pki/imx6ul_hab_testkeys/IMG1_1_sha256_4096_65537_v3_usr_crt.pem)

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

plat_final:
	$(CSF_SIGN_TOOL) --csf_key $(PB_CSF_KEY) \
					 --csf_crt $(PB_CSF_CRT) \
					 --img_key $(PB_IMG_KEY) \
					 --img_crt $(PB_IMG_CRT) \
					 --table $(PB_SRK_TABLE) \
					 --index 1 \
					 --image $(BUILD_DIR)/pb.imx \
					 --output $(BUILD_DIR)/pb_csf.bin

	@cat $(BUILD_DIR)/pb.imx $(BUILD_DIR)/pb_csf.bin > $(BUILD_DIR)/pb_signed.imx

	$(CSF_SIGN_TOOL) --csf_key $(PB_CSF_KEY) \
					 --csf_crt $(PB_CSF_CRT) \
					 --img_key $(PB_IMG_KEY) \
					 --img_crt $(PB_IMG_CRT) \
					 --table $(PB_SRK_TABLE) \
					 --index 1 \
					 --serial \
					 --image $(BUILD_DIR)/pb.imx \
					 --output $(BUILD_DIR)/pb_csf_uuu.bin

	@cat $(BUILD_DIR)/pb.imx $(BUILD_DIR)/pb_csf_uuu.bin > $(BUILD_DIR)/pb_signed_uuu.imx

