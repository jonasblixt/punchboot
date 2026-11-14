/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The CAAM driver (src/drivers/crypto/caam/imx_caam.c) on a CAAM with the
 * low power MDHA (MDHA LP256: SHA-256 at most) before era 10, as the model
 * has it for the i.MX6UL: the driver registers its hash without SHA-384 and
 * SHA-512, so the crypto API refuses them and pb doesn't send the engine a
 * job it can't do. test_caam_crypto.c has the CAAM with the high performance
 * MDHA. The driver can be set up once per program, so this is its own.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "crypto_vectors.h"
#include "fakes/dcache.h"
#include "models/bench.h"
#include "models/caam_model.h"
#include <drivers/crypto/imx_caam.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <string.h>

/* Job ring 0 of the i.MX6UL */
#define CAAM_JR_BASE 0x02141000

/* What the engine reads and writes is below 4 GiB (caam_model.h): static */
static uint8_t abc[3] = { 'a', 'b', 'c' };
static uint8_t digest[CRYPTO_MD_MAX_SZ];

uint64_t fake_mmio_read(uintptr_t addr, unsigned int width)
{
    if (!caam_model_owns(addr) || width != 32)
        fail_msg("Unexpected %u bit read at 0x%lx", width, (unsigned long)addr);
    return caam_model_read(addr);
}

void fake_mmio_write(uintptr_t addr, uint64_t value, unsigned int width)
{
    if (!caam_model_owns(addr) || width != 32)
        fail_msg("Unexpected %u bit write at 0x%lx", width, (unsigned long)addr);
    caam_model_write(addr, (uint32_t)value);
}

static int group_setup(void **state)
{
    static uint32_t earlier_ring[2];
    static const struct caam_model_config cfg = {
        .jr_base = CAAM_JR_BASE,
        .era = 9,
        .lp256 = true,
    };
    (void)state;

    bench_reset();
    dcache_reset();
    caam_model_init(&cfg);
    caam_model_earlier_stage(earlier_ring, 0);

    if (imx_caam_init(CAAM_JR_BASE) != PB_OK)
        return -1;

    return 0;
}

/* SHA-256 and MD5 are the engine's */
static void test_sha256(void **state)
{
    (void)state;

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update(abc, sizeof(abc)), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);
    assert_memory_equal(digest, vector_sha256_abc, 32);

    assert_int_equal(hash_init(HASH_MD5), PB_OK);
    assert_int_equal(hash_update(abc, sizeof(abc)), PB_OK);
    assert_int_equal(hash_final(digest, 16), PB_OK);
    assert_memory_equal(digest, vector_md5_abc, 16);
    assert_true(caam_model_idle());
}

/* SHA-384 and SHA-512 aren't: refused before a job, and an image or token
 * with them fails. The RT1060 is in the same place with Mbed TLS (F-286
 * (10)): a P-384 or P-521 key that signs with them verifies nothing */
static void test_no_sha384_sha512(void **state)
{
    unsigned int jobs = caam_model_jobs_done();
    (void)state;

    assert_int_equal(hash_init(HASH_SHA384), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(hash_init(HASH_SHA512), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(caam_model_jobs_done(), jobs);
    assert_true(caam_model_idle());

    /* SHA-256 still works after them */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update(abc, sizeof(abc)), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);
    assert_memory_equal(digest, vector_sha256_abc, 32);
}

static void test_no_violations(void **state)
{
    (void)state;
    assert_int_equal(bench_violations(), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sha256),
        cmocka_unit_test(test_no_sha384_sha512),
        cmocka_unit_test(test_no_violations),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
