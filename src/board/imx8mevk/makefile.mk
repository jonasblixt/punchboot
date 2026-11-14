src-y += $(BOARD)/board.c
src-y += $(BOARD)/lpddr4_timing.c

# The board's memory (platform.md 5.1): where pb is linked, pb's region,
# the DRAM and pb's buffers. The link checks them against the SoC.
# pb in the TCM, to the end of the ROM's load window. The buffers are the
# platform's.
PB_ENTRY        = 0x7e1000
BOARD_DRAM_BASE = 0x40000000
BOARD_DRAM_SIZE = 0x80000000

# The LPDDR4 training firmware is the platform's (src/plat/imx8m/makefile.mk)

# The image stages are the i.MX8M's (src/plat/imx8m/makefile.mk), turned on
# and given the keys in the defconfig
