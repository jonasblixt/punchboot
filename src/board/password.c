/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <board/password.h>
#include <pb/pb.h>
#include <stdint.h>

int board_password_check(const char *expected,
                         size_t expected_length,
                         const char *password,
                         size_t length)
{
    volatile uint8_t diff;

    if (expected == NULL || expected_length == 0 || password == NULL || length == 0)
        return -PB_ERR_AUTHENTICATION_FAILED;

    diff = (uint8_t)(length != expected_length);

    /* Every byte the host sent is compared. Past the end of the expected
     * password the lengths differ already: its first byte is read instead,
     * picked with a mask and not a branch */
    for (size_t i = 0; i < length; i++) {
        size_t in_expected = (size_t)0 - (size_t)(i < expected_length);

        diff |= (uint8_t)(password[i] ^ expected[i & in_expected]);
    }

    return (diff == 0) ? PB_OK : -PB_ERR_AUTHENTICATION_FAILED;
}
