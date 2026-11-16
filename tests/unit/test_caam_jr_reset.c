/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The CAAM driver's job ring reset at init (drivers.md 2.1, F-410): an
 * earlier user of the job ring (the ROM, a stage before pb) left a
 * completion in its output ring and a job in the engine that doesn't end by
 * itself. The init flushes the ring, which ends that job into the earlier
 * user's output ring once the driver has made room, and resets it before it
 * sets up its own rings. A flush that never completes fails the init, and
 * nothing is registered. The driver can be set up once per program after a
 * success, so this is its own: the failure first, then the init that works.
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
#define IRBAR_LS     0x004
#define ORBAR_LS     0x024

/* What the engine reads and writes is below 4 GiB (caam_model.h): static */
static uint32_t earlier_ring[2];
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
    static const struct caam_model_config cfg = {
        .jr_base = CAAM_JR_BASE,
        .era = 8,
    };
    (void)state;

    bench_reset();
    dcache_reset();
    caam_model_init(&cfg);
    caam_model_earlier_job(earlier_ring);
    return 0;
}

/* The flush doesn't complete: the init gives up after its bound, leaves the
 * earlier user's rings as they are and registers no hash */
static void test_a_flush_that_never_completes(void **state)
{
    (void)state;

    caam_model_flush_stuck(true);
    assert_int_equal(imx_caam_init(CAAM_JR_BASE), -PB_ERR_TIMEOUT);
    assert_int_equal(caam_model_read(CAAM_JR_BASE + IRBAR_LS), 0);
    assert_int_equal(caam_model_read(CAAM_JR_BASE + ORBAR_LS), (uint32_t)(uintptr_t)earlier_ring);
    assert_int_not_equal(hash_init(HASH_SHA256), PB_OK);
    assert_bench_clean();
}

/* The flush completes: the earlier user's job ends in its own ring, after
 * the completion it left there was taken out, then the ring is reset and
 * the driver's rings are set up. The earlier ring's flush is still under way
 * from the first init, so the driver doesn't ask for it again */
static void test_init_flushes_and_resets(void **state)
{
    (void)state;

    caam_model_flush_stuck(false);
    assert_int_equal(imx_caam_init(CAAM_JR_BASE), PB_OK);
    assert_int_equal(earlier_ring[1], CAAM_STATUS_FLUSHED);
    assert_int_not_equal(caam_model_read(CAAM_JR_BASE + ORBAR_LS),
                         (uint32_t)(uintptr_t)earlier_ring);
    assert_bench_clean();

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update(abc, sizeof(abc)), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);
    assert_memory_equal(digest, vector_sha256_abc, 32);
    assert_true(caam_model_idle());
    assert_bench_clean();
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_a_flush_that_never_completes),
        cmocka_unit_test(test_init_flushes_and_resets),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
