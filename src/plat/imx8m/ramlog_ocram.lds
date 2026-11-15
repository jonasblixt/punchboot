/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8MQ's RAM log (CONFIG_RAMLOG) in the OCRAM, at its start: pb in
 * the TCM, its code and the LPDDR4 training firmware, leaves no room for it,
 * and it is written before the DRAM is trained. The 8MQ RM (6.1.4.1): "The
 * entire OCRAM region can be used freely after the boot". TF-A's BL31 is
 * linked at 0x910000, after it. Not loaded, not zeroed (ramlog.c).
 */

MEMORY
{
    ramlog_ocram (rw) : ORIGIN = 0x00900000, LENGTH = 0x10000
}

SECTIONS
{
    .ramlog_ocram (NOLOAD):
    {
        *(.ramlog)
    } > ramlog_ocram
}
