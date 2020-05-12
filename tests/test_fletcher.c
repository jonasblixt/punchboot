/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <plat/qemu/semihosting.h>
#include <pb/fletcher.h>
#include <pb/assert.h>
#include "test.h"

void test_main(void)

{
    uint16_t res;
    LOG_INFO("Fletcher checksum test");

    uint8_t test_vector1[] = "abcde";
    res = fletcher8(test_vector1, 5);
    LOG_INFO("fletcher8(test_vector1[]) = 0x%x", res);
    assert(res == 0xc8f0);

    uint8_t test_vector2[] = "";
    res = fletcher8(test_vector2, 0);
    LOG_INFO("fletcher8(test_vector2[]) = 0x%x", res);
    assert(res == 0xffff);

    uint8_t test_vector3[] = "abcdef";
    res = fletcher8(test_vector3, 6);
    LOG_INFO("fletcher8(test_vector3[]) = 0x%x", res);
    assert(res == 0x2057);

    uint8_t test_vector4[] = "test123";
    res = fletcher8(test_vector4, 7);
    LOG_INFO("fletcher8(test_vector4[]) = 0x%x", res);
    assert(res == 0xcd58);

    uint8_t test_vector5[] = "\x02\x10";
    res = fletcher8(test_vector5, 2);
    LOG_INFO("fletcher8(test_vector5[]) = 0x%x", res);
    assert(res == 0x1412);

    LOG_INFO("Fletcher checksum test END");
}
