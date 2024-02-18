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
    ram (rw)  : ORIGIN = 0x20249000, LENGTH = 256k
    buffers (rw)  : ORIGIN = 0x20289000, LENGTH = 220k
}
