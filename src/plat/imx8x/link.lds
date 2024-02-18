/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

MEMORY
{
    reset(rx) : ORIGIN = 0x80000000, LENGTH = 128K
    ram (rx)  : ORIGIN = 0x80020000, LENGTH = 16M
    buffers (rx)  : ORIGIN = 0x85000000, LENGTH = 16M
}

/**
 * Provide a 128 kByte hole for loading ATF at 0x80000000.
 * The SCFW code seems to have a hardcoded boot vector to that address.
 * The NXP supplied ATF also has a lot of code that assumes that ATF
 * is loaded at that particular address.
 */

SECTIONS
{
    . = 0x80000000;

    .imx8x_reset :
    {
        PROVIDE (_imx8x_start = .);
        KEEP(*(.text.imx8x_reset))
        . = ALIGN(8);
        PROVIDE (_imx8x_end = .);
    } > reset

}
