src-y += $(BOARD)/board.c
src-y += $(BOARD)/dram.c
src-y += $(BOARD)/console.c

# The SPL's (board.md 7, D-088): the DRAM's rates, its straps and its timing
spl-src-y += $(BOARD)/dram.c
spl-src-y += $(BOARD)/lpddr4_timing_1gb.c
spl-src-y += $(BOARD)/lpddr4_timing_2gb.c
spl-src-y += $(BOARD)/lpddr4_timing_3gb.c
spl-src-y += $(BOARD)/lpddr4_timing_4gb.c
spl-src-y += src/drivers/gpio/imx_gpio.c
# The SPL's console (plat/imx8m/spl.h)
spl-src-y += $(BOARD)/console.c
spl-src-y += src/drivers/uart/imx_uart.c

# The board's memory (platform.md 5.1): where pb is linked, pb's region,
# the DRAM and pb's buffers. The link checks them against the SoC.
# pb proper in DRAM after the buffers (D-088), 2 MiB; the SPL is the
# platform's, in the TCM. The ROM loads the image's start below PB_ENTRY,
# after the buffers, which are the platform's. The DRAM is 1 to 4 GiB,
# strapped (dram.c): pb uses the first GiB, which every variant has (D-079).
PB_ENTRY        = 0x41100000
PB_SIZE         = 0x200000
BOARD_DRAM_BASE = 0x40000000
BOARD_DRAM_SIZE = 0x40000000

# The LPDDR4 training firmware is the platform's (src/plat/imx8m/makefile.mk)

# The image stages are the i.MX8M's (src/plat/imx8m/makefile.mk), turned on
# and given the keys in the defconfig
