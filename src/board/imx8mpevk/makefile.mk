#
# Punch BOOT
#
# Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
#
# SPDX-License-Identifier: BSD-3-Clause
#
# i.MX8M Plus EVK. pb runs from OCRAM at 0x920000, after the ROM's part of it.

src-y += $(BOARD)/board.c
src-y += $(BOARD)/dram.c

# The SPL's (board.md 7, D-088): the DRAM's rates and its timing
spl-src-y += $(BOARD)/dram.c
spl-src-y += $(BOARD)/lpddr4_timing.c

# The board's memory (platform.md 5.1): where pb is linked, pb's region,
# the DRAM and pb's buffers. The link checks them against the SoC.
# pb proper in DRAM after the buffers (D-088), 2 MiB; the SPL is the
# platform's, in on-chip RAM. The ROM or the SPL loads the image's start
# below PB_ENTRY, after the buffers, which are the platform's.
PB_ENTRY        = 0x41100000
PB_SIZE         = 0x200000
BOARD_DRAM_BASE = 0x40000000
BOARD_DRAM_SIZE = 0x80000000

# The LPDDR4 training firmware is the platform's (src/plat/imx8m/makefile.mk).
# lpddr4_timing.c is FW2020.06's: give it NXP's *_202006.bin files.

# The image stages are the i.MX8M's (src/plat/imx8m/makefile.mk), turned on
# in the defconfig: pb.imx for SD/eMMC or the serial downloader (uuu)
