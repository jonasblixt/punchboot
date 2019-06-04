
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

MEMORY
{

    pbram (rx)  : ORIGIN = 0x7E1000, LENGTH = 124K
    pbram2 (rx) : ORIGIN = 0x180000, LENGTH = 32K
    dram (rx)   : ORIGIN = 0x40000000, LENGTH = 128M

/*
    pbram (rx)  : ORIGIN = 0x44000000, LENGTH = 16M
    pbram2 (rx) : ORIGIN = 0x45000000, LENGTH = 16M
    dram (rx)   : ORIGIN = 0x46000000, LENGTH = 128M
*/

}
