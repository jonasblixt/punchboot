src-y += $(BOARD)/board.c
src-y += src/board/password.c

# The board's memory (platform.md 5.1): where pb is linked, pb's region,
# the DRAM and pb's buffers. The link checks them against the SoC.
# pb after BL31 at the DRAM's start, with an image data region between pb's
# 512 KiB and its buffers (board.c). The buffers are the platform's.
PB_ENTRY        = 0x80000000
PB_SIZE         = 0x80000
BOARD_DRAM_BASE = 0x80000000
BOARD_DRAM_SIZE = 0xc0000000

# The image stages are the i.MX8X's (src/plat/imx8x/makefile.mk), turned on
# and given the keys in the defconfig. The board's input is its SCU firmware.
IMX8X_SCFW_IMAGE ?= mx8qx-mek-scfw-tcm.bin
