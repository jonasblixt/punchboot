/**
 *
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <plat/qemu/semihosting.h>
#include <pb/usb.h>
#include <pb/assert.h>
#include <string.h>
#include "test.h"

void test_main(void)

{
    char s[256];

    LOG_INFO("Boot libc");

    snprintf(s, sizeof(s), "%i", -1234567890);
    assert(strcmp(s, "-1234567890") == 0);

    snprintf(s, sizeof(s), "%08x", 0x0000CAFE);
    LOG_INFO("%s", s);
    assert(strcmp(s, "0000cafe") == 0);


    snprintf(s, sizeof(s), "%x", 0xCAFEBABE);
    assert(strcmp(s, "cafebabe") == 0);
    LOG_INFO("Boot libc end");
}
