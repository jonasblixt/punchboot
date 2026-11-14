/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Where pb, its buffers and the board's DRAM are is the board's, from its
 * defconfig (platform.md 5.1): PB_ENTRY, PB_SIZE, PB_BUFFERS_BASE and _SIZE,
 * BOARD_DRAM_BASE and _SIZE (none on a board without DRAM). The addresses
 * here are the SoC's.
 */

/* The ROM's load window: OCRAM2 above the ROM's own data, 32 KiB that its
 * API still uses, and the FlexRAM's OCRAM after it, 256 KiB with the
 * FlexRAM's default configuration, which pb doesn't change
 * (plat/imxrt/mm.h) */
IMXRT_LOAD_BASE = 0x20208000;
IMXRT_LOAD_END = 0x202c0000;

/* The DTCM, with that configuration */
IMXRT_DTCM_BASE = 0x20000000;
IMXRT_DTCM_END = 0x20020000;

/* pb has the window from PB_ENTRY to the buffers, when they follow it
 * there, and else to the window's end; a board with PB_SIZE gives it less.
 *
 * The length is written out in MEMORY: a symbol that this script makes from
 * the board's values isn't known yet when ld works MEMORY out, and the
 * region then has every address, without a message. make.link checks the
 * regions. */
MEMORY
{
    ram (rw)  : ORIGIN = PB_ENTRY,
                LENGTH = (PB_SIZE != 0 ? PB_ENTRY + PB_SIZE :
                          (PB_BUFFERS_BASE > PB_ENTRY && PB_BUFFERS_BASE < IMXRT_LOAD_END ?
                           PB_BUFFERS_BASE : IMXRT_LOAD_END)) - PB_ENTRY
    buffers (rw)  : ORIGIN = PB_BUFFERS_BASE, LENGTH = PB_BUFFERS_SIZE
}

BOARD_DRAM_END = BOARD_DRAM_BASE + BOARD_DRAM_SIZE;
PB_REGION_END = ORIGIN(ram) + LENGTH(ram);

ASSERT(PB_ENTRY >= IMXRT_LOAD_BASE && PB_ENTRY < IMXRT_LOAD_END,
       "CONFIG_PB_ENTRY is not in the on-chip RAM the i.MX RT's ROM loads pb to")

ASSERT(PB_REGION_END > PB_ENTRY && PB_REGION_END <= IMXRT_LOAD_END,
       "CONFIG_PB_SIZE ends pb's region after the memory the ROM loads pb to")

/* Memory that works after plat_init(): the window (the ROM's data stays the
 * ROM's), the DTCM and the board's DRAM. Not the ITCM, from address 0 */
ASSERT((PB_BUFFERS_BASE >= IMXRT_LOAD_BASE &&
        PB_BUFFERS_BASE + PB_BUFFERS_SIZE <= IMXRT_LOAD_END) ||
       (PB_BUFFERS_BASE >= IMXRT_DTCM_BASE &&
        PB_BUFFERS_BASE + PB_BUFFERS_SIZE <= IMXRT_DTCM_END) ||
       (PB_BUFFERS_BASE >= BOARD_DRAM_BASE &&
        PB_BUFFERS_BASE + PB_BUFFERS_SIZE <= BOARD_DRAM_END),
       "pb's buffers (CONFIG_PB_BUFFERS_BASE, _SIZE) are not in the OCRAM, the DTCM or the board's DRAM")

ASSERT(ORIGIN(buffers) + LENGTH(buffers) <= ORIGIN(ram) ||
       ORIGIN(buffers) >= ORIGIN(ram) + LENGTH(ram),
       "pb's buffers (CONFIG_PB_BUFFERS_BASE, _SIZE) overlap pb")

/* The Firmware Handoff region is the board's too (CONFIG_BOOT_FH): in the
 * same memory. src/boot/fh.lds keeps it off pb and the buffers */
BOOT_FH_REGION_END = DEFINED(BOOT_FH_REGION_BASE) ? BOOT_FH_REGION_BASE + BOOT_FH_REGION_SIZE : 0;
ASSERT(DEFINED(BOOT_FH_REGION_BASE) ?
       ((BOOT_FH_REGION_BASE >= IMXRT_LOAD_BASE && BOOT_FH_REGION_END <= IMXRT_LOAD_END) ||
        (BOOT_FH_REGION_BASE >= IMXRT_DTCM_BASE && BOOT_FH_REGION_END <= IMXRT_DTCM_END) ||
        (BOOT_FH_REGION_BASE >= BOARD_DRAM_BASE && BOOT_FH_REGION_END <= BOARD_DRAM_END)) : 1,
       "The Firmware Handoff region (CONFIG_BOOT_FH_REGION_BASE, _SIZE) is not in the OCRAM, the DTCM or the board's DRAM")
