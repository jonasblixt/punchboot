/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The core's software crypto backend (src/crypto_sw.c, crypto.md 2, D-086),
 * with a fake Mbed TLS. Built twice: with CONFIG_DRIVERS_CRYPTO_MBEDTLS, as
 * the host's configuration has it, and without (test_crypto_sw_no_mbedtls).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/crypto.h>
#include <pb/pb.h>

static int mbedtls_rc;
static unsigned int mbedtls_inits;

int mbedtls_pb_init(void)
{
    mbedtls_inits++;
    return mbedtls_rc;
}

/* Before the set up nothing is: the boot's preconditions read none */
static void test_none_before_the_init(void **state)
{
    (void)state;

    assert_int_equal(crypto_sw_state(), CRYPTO_SW_NONE);
}

/* With the option Mbed TLS is set up once per call, and its result is the
 * state; without it nothing is, and the state stays none */
static void test_init(void **state)
{
    (void)state;

    mbedtls_rc = -PB_ERR_MEM;
    crypto_sw_init();
#ifdef CONFIG_DRIVERS_CRYPTO_MBEDTLS
    assert_int_equal(mbedtls_inits, 1);
    assert_int_equal(crypto_sw_state(), CRYPTO_SW_FAILED);
#else
    assert_int_equal(mbedtls_inits, 0);
    assert_int_equal(crypto_sw_state(), CRYPTO_SW_NONE);
#endif

    mbedtls_rc = PB_OK;
    crypto_sw_init();
#ifdef CONFIG_DRIVERS_CRYPTO_MBEDTLS
    assert_int_equal(mbedtls_inits, 2);
    assert_int_equal(crypto_sw_state(), CRYPTO_SW_OK);
#else
    assert_int_equal(mbedtls_inits, 0);
    assert_int_equal(crypto_sw_state(), CRYPTO_SW_NONE);
#endif
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_none_before_the_init),
        cmocka_unit_test(test_init),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
