/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Controls for the fake software crypto backend.
 */

#ifndef TESTS_UNIT_FAKES_CRYPTO_SW_H
#define TESTS_UNIT_FAKES_CRYPTO_SW_H

#include <pb/crypto.h>

/* What crypto_sw_state() answers, CRYPTO_SW_OK by default */
extern enum crypto_sw_state fake_crypto_sw_state;
/* Number of crypto_sw_init() calls */
extern unsigned int fake_crypto_sw_init_calls;

#endif // TESTS_UNIT_FAKES_CRYPTO_SW_H
