/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The core's software crypto backend (src/crypto_sw.c), for the tests of what
 * asks for its state: the boot's preconditions, command mode's log. A test
 * that links src/crypto_sw.c gets the real one: this file is a member of the
 * fakes' archive of its own.
 */

#include "crypto_sw.h"

enum crypto_sw_state fake_crypto_sw_state = CRYPTO_SW_OK;
unsigned int fake_crypto_sw_init_calls;

void crypto_sw_init(void)
{
    fake_crypto_sw_init_calls++;
}

enum crypto_sw_state crypto_sw_state(void)
{
    return fake_crypto_sw_state;
}
