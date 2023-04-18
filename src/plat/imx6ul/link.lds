/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


/**
 * ERR011115: User should set the boot image start address greater or
 *   equal to 0x908000 (if the boot image running in OCRAM)
 */

MEMORY
{
    ram (rx)  : ORIGIN = 0x80001000, LENGTH = 1M
    buffers (rx)  : ORIGIN = 0x81021000, LENGTH = 16M
}
