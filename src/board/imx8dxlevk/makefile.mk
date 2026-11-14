src-y += $(BOARD)/board.c
src-y += src/board/password.c

# The board's memory (platform.md 5.1): where pb is linked, pb's region,
# the DRAM and pb's buffers. The link checks them against the SoC.
# pb after BL31 at the DRAM's start, with an image data region between pb's
# 512 KiB and its buffers (board.c). The buffers are the platform's.
# The EVK has 1 GiB of LPDDR4.
PB_ENTRY        = 0x80000000
PB_SIZE         = 0x80000
BOARD_DRAM_BASE = 0x80000000
BOARD_DRAM_SIZE = 0x40000000

# The image stages are the i.MX8X's (src/plat/imx8x/makefile.mk), turned on
# in the defconfig. The board's input is its SCU firmware. IMX8XL_SCFW_IMAGE
# and IMX8XL_AHAB_IMAGE are the names before 3.0.0, which a command line may
# still give.
IMX8XL_SCFW_IMAGE ?= mx8dxl-evk-scfw-tcm.bin
IMX8X_SCFW_IMAGE ?= $(IMX8XL_SCFW_IMAGE)
ifdef IMX8XL_AHAB_IMAGE
IMX8X_AHAB_IMAGE ?= $(IMX8XL_AHAB_IMAGE)
endif
