src-y += $(BOARD)/board.c
src-y += $(BOARD)/lpddr4_timing_common.c
src-y += $(BOARD)/lpddr4_timing_1gb.c
src-y += $(BOARD)/lpddr4_timing_2gb.c
src-y += $(BOARD)/lpddr4_timing_3gb.c
src-y += $(BOARD)/lpddr4_timing_4gb.c

# The board's memory (platform.md 5.1): where pb is linked, pb's region,
# the DRAM and pb's buffers. The link checks them against the SoC.
# pb in the TCM, to the end of the ROM's load window. The buffers are the
# platform's. The DRAM is 1 to 4 GiB, strapped (board.c): pb uses the
# first GiB, which every variant has (D-079).
PB_ENTRY        = 0x7e1000
BOARD_DRAM_BASE = 0x40000000
BOARD_DRAM_SIZE = 0x40000000

# The LPDDR4 training firmware is the platform's (src/plat/imx8m/makefile.mk)

# The image stages are the i.MX8M's (src/plat/imx8m/makefile.mk), turned on
# and given the keys in the defconfig
