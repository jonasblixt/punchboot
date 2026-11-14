src-y += $(BOARD)/board.c
src-y += src/board/password.c

# The image stages are the i.MX8X's (src/plat/imx8x/makefile.mk), turned on
# and given the keys in the defconfig. The board's input is its SCU firmware.
IMX8X_SCFW_IMAGE ?= mx8qx-mek-scfw-tcm.bin
