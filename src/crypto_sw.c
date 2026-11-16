/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The software crypto backend is the core's (crypto.md 2, D-086): set up the
 * same on every platform, right after plat_init(), behind its driver's
 * option. The SoC's engine stays the platform's (platform.md 2.2).
 */

#include <pb/crypto.h>
#include <pb/pb.h>

#ifdef CONFIG_DRIVERS_CRYPTO_MBEDTLS
#include <drivers/crypto/mbedtls.h>
#endif

static enum crypto_sw_state state = CRYPTO_SW_NONE;

void crypto_sw_init(void)
{
#ifdef CONFIG_DRIVERS_CRYPTO_MBEDTLS
    /* Command mode can start without it, so it isn't main()'s reset: it is
     * recorded, and the boot's preconditions refuse the boot (platform.md
     * 2.3) */
    int rc = mbedtls_pb_init();

    if (rc != PB_OK) {
        LOG_ERR("Mbed TLS init failed (%i)", rc);
        state = CRYPTO_SW_FAILED;
    } else {
        state = CRYPTO_SW_OK;
    }
#endif
}

enum crypto_sw_state crypto_sw_state(void)
{
    return state;
}
