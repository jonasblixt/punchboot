src-y += $(BOARD)/board.c
src-y += src/board/password.c

# The image stages are the i.MX8X's (src/plat/imx8x/makefile.mk), turned on
# in the defconfig. The board's input is its SCU firmware. IMX8XL_SCFW_IMAGE
# and IMX8XL_AHAB_IMAGE are the names before 3.0.0, which a command line may
# still give.
IMX8XL_SCFW_IMAGE ?= mx8dxl-evk-scfw-tcm.bin
IMX8X_SCFW_IMAGE ?= $(IMX8XL_SCFW_IMAGE)
ifdef IMX8XL_AHAB_IMAGE
IMX8X_AHAB_IMAGE ?= $(IMX8XL_AHAB_IMAGE)
endif
