/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * RNG4 instantiation in the CAAM driver (src/drivers/crypto/caam/imx_caam.c)
 * against a model of the CAAM's controller page, one job ring and the RNG's
 * state handles. The model runs the RNG operations of a job descriptor.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "fakes/plat.h"
#include <drivers/crypto/imx_caam.h>
#include <pb/pb.h>
#include <string.h>

/* Controller page, job ring 0 at +0x1000 as on the i.MX6UL and i.MX8M */
#define JR0           0x1000
#define IRBAR_LS      (JR0 + 0x0004)
#define IRJAR         (JR0 + 0x001c)
#define ORBAR_LS      (JR0 + 0x0024)
#define ORJRR         (JR0 + 0x0034)
#define ORSFR         (JR0 + 0x003c)
#define JRSTAR        (JR0 + 0x0044)
#define RNG_VERSION   (JR0 + 0x0ef8)
#define CTPR_MS       (JR0 + 0x0fa8)
#define CCBVID        (JR0 + 0x0fe4)
#define CHAVID_LS     (JR0 + 0x0fec)
#define RTMCTL        0x0600
#define RTSDCTL       0x0610
#define RTFRQMIN      0x0618
#define RTFRQMAX      0x061c
#define RDSTA         0x06c0

#define RTMCTL_PRGM   BIT(16)
#define RTMCTL_ERR    BIT(12)
#define RTMCTL_ACC    BIT(5)
#define RDSTA_IF0     BIT(0)
#define RDSTA_IF1     BIT(1)
#define RDSTA_PR0     BIT(4)
#define RDSTA_PR1     BIT(5)
#define RDSTA_SKVN    BIT(30)
#define RDSTA_ALL     (RDSTA_IF0 | RDSTA_IF1 | RDSTA_PR0 | RDSTA_PR1 | RDSTA_SKVN)

/* A CCB error from the RNG, as when the TRNG had too little entropy */
#define STATUS_RNG_HW 0x2000005b

/* The RNG operations the model ran, in order */
enum op {
    OP_INST0,
    OP_INST1,
    OP_UNINST0,
    OP_UNINST1,
    OP_SECKEYS,
};

/* The driver's job ring entries hold 32 bit addresses: link with -no-pie */
static uint32_t regs[0x2000 / 4];

static struct {
    unsigned int jobs;
    unsigned int kicks;
    uint32_t ent_delay[64]; /* Entropy delay of each kick */
    enum op ops[64];
    unsigned int nr_ops;
    unsigned int fail_inst; /* Instantiations that fail before one works */
    bool hang; /* Jobs never complete */
} model;

#define REG(off) (regs[(off) / 4])

static uint32_t *ptr(uint32_t addr)
{
    return (uint32_t *)(uintptr_t)addr;
}

/* Runs the descriptor and writes the output ring entry */
static void run_job(void)
{
    uint32_t *desc = ptr(*ptr(REG(IRBAR_LS)));
    uint32_t len = desc[0] & 0x7f;
    uint32_t status = 0;

    assert_int_equal(desc[0] >> 27, 0x16); /* Job descriptor header */
    model.jobs++;

    for (uint32_t i = 1; i < len && status == 0; i++) {
        uint32_t cmd = desc[i];

        switch (cmd >> 27) {
        case 0x10: { /* OPERATION */
            uint32_t sh = (cmd >> 4) & 1;
            uint32_t as = (cmd >> 2) & 3;

            assert_int_equal((cmd >> 16) & 0xff, 0x50); /* RNG */
            assert_int_equal((cmd >> 24) & 7, 2); /* Class 1 */

            if (cmd & BIT(12)) {
                assert_int_equal(as, 0);
                /* Only once per power cycle, and with state handle 0 */
                assert_false(REG(RDSTA) & RDSTA_SKVN);
                assert_true(REG(RDSTA) & RDSTA_IF0);
                REG(RDSTA) |= RDSTA_SKVN;
                model.ops[model.nr_ops++] = OP_SECKEYS;
            } else if (as == 1) {
                assert_true(cmd & BIT(1)); /* Prediction resistance */
                assert_false(REG(RDSTA) & (RDSTA_IF0 << sh));
                /* Nothing to instantiate from while the TRNG is programmed */
                assert_false(REG(RTMCTL) & RTMCTL_PRGM);
                model.ops[model.nr_ops++] = sh ? OP_INST1 : OP_INST0;

                if (model.fail_inst > 0) {
                    model.fail_inst--;
                    status = STATUS_RNG_HW;
                } else {
                    REG(RDSTA) |= (RDSTA_IF0 | RDSTA_PR0) << sh;
                }
            } else if (as == 3) {
                REG(RDSTA) &= ~((RDSTA_IF0 | RDSTA_PR0) << sh);
                model.ops[model.nr_ops++] = sh ? OP_UNINST1 : OP_UNINST0;
            } else {
                fail_msg("Unexpected RNG operation 0x%08x", cmd);
            }
            break;
        }
        case 0x14: /* JUMP: wait for class 1 done */
            assert_int_equal(cmd & 0xff, 1);
            break;
        case 0x02: /* LOAD immediate: clear written */
            assert_true(cmd & BIT(23));
            assert_int_equal((cmd >> 16) & 0x7f, 0x08);
            assert_int_equal(desc[++i], 1);
            break;
        default:
            fail_msg("Unexpected command 0x%08x", cmd);
        }
    }

    uint32_t *out = ptr(REG(ORBAR_LS));

    out[0] = *ptr(REG(IRBAR_LS));
    out[1] = status;
    REG(JRSTAR) = status;
    REG(ORSFR) = 1;
}

uint64_t fake_mmio_read(uintptr_t addr, unsigned int width)
{
    uintptr_t off = addr - (uintptr_t)regs;

    assert_int_equal(width, 32);
    assert_true(addr >= (uintptr_t)regs && off < sizeof(regs));

    /* Reads zero outside program mode */
    if (off == RTSDCTL && !(REG(RTMCTL) & RTMCTL_PRGM))
        return 0;

    return REG(off);
}

void fake_mmio_write(uintptr_t addr, uint64_t value, unsigned int width)
{
    uintptr_t off = addr - (uintptr_t)regs;

    assert_int_equal(width, 32);
    assert_true(addr >= (uintptr_t)regs && off < sizeof(regs));

    switch (off) {
    case IRJAR:
        if (!model.hang)
            run_job();
        break;
    case ORJRR:
        REG(ORSFR) = 0;
        break;
    case RTSDCTL:
        /* Only in program mode, and only while no state handle is instantiated */
        assert_true(REG(RTMCTL) & RTMCTL_PRGM);
        assert_false(REG(RDSTA) & (RDSTA_IF0 | RDSTA_IF1));
        model.ent_delay[model.kicks++] = (uint32_t)value >> 16;
        REG(off) = (uint32_t)value;
        break;
    case RTMCTL:
        /* ERR is write 1 to clear */
        REG(off) = ((uint32_t)value & ~RTMCTL_ERR) | (REG(off) & RTMCTL_ERR & ~(uint32_t)value);
        break;
    default:
        REG(off) = (uint32_t)value;
    }
}

void arch_invalidate_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

/* An era 9 CAAM with RNG4 and two state handles, as on the i.MX8M */
static int setup(void **state)
{
    /* The driver sets up the job ring once, in imx_caam_init() */
    uint32_t irbar = REG(IRBAR_LS);
    uint32_t orbar = REG(ORBAR_LS);

    (void)state;
    memset(regs, 0, sizeof(regs));
    REG(IRBAR_LS) = irbar;
    REG(ORBAR_LS) = orbar;
    memset(&model, 0, sizeof(model));
    REG(CCBVID) = 9 << 24;
    REG(CHAVID_LS) = (4 << 16) | (1 << 12);
    REG(CTPR_MS) = 2 << 8;
    REG(RTMCTL) = RTMCTL_ERR; /* As the ROM may leave it */
    fake_us_tick_step = 0;
    return 0;
}

static void assert_ops(const enum op *ops, unsigned int n)
{
    assert_int_equal(model.nr_ops, n);
    for (unsigned int i = 0; i < n; i++)
        assert_int_equal(model.ops[i], ops[i]);
}

/* Runs first: the driver keeps its state between tests */
static void test_not_initialized(void **state)
{
    (void)state;
    assert_int_equal(imx_caam_rng_init((uintptr_t)regs), -PB_ERR_STATE);
    assert_int_equal(imx_caam_init((uintptr_t)regs + JR0), PB_OK);
}

static void test_instantiate(void **state)
{
    static const enum op ops[] = { OP_INST0, OP_SECKEYS, OP_INST1 };

    (void)state;
    assert_int_equal(imx_caam_rng_init((uintptr_t)regs), PB_OK);
    assert_int_equal(REG(RDSTA), RDSTA_ALL);
    assert_ops(ops, ARRAY_SIZE(ops));
    assert_int_equal(model.jobs, 2);

    /* Programmed once, at the shortest entropy delay, then back in run mode
     * with raw sampling and the error bit cleared */
    assert_int_equal(model.kicks, 1);
    assert_int_equal(model.ent_delay[0], 3200);
    assert_int_equal(REG(RTSDCTL) & 0xffff, 512);
    assert_int_equal(REG(RTFRQMIN), 3200 / 4);
    assert_int_equal(REG(RTFRQMAX), 3200 * 16);
    assert_int_equal(REG(RTMCTL) & (RTMCTL_PRGM | RTMCTL_ACC | RTMCTL_ERR | 3), 1);
}

/* E.g. after a warm reset: nothing to do */
static void test_already_instantiated(void **state)
{
    (void)state;
    REG(RDSTA) = RDSTA_ALL;
    assert_int_equal(imx_caam_rng_init((uintptr_t)regs), PB_OK);
    assert_int_equal(model.jobs, 0);
    assert_int_equal(model.kicks, 0);
}

/* State handle 0 without prediction resistance, e.g. from the ROM: Linux
 * would tear it down, which it can't in the normal world */
static void test_without_prediction_resistance(void **state)
{
    static const enum op ops[] = { OP_UNINST0, OP_INST0, OP_INST1 };

    (void)state;
    REG(RDSTA) = RDSTA_IF0 | RDSTA_SKVN;
    assert_int_equal(imx_caam_rng_init((uintptr_t)regs), PB_OK);
    assert_int_equal(REG(RDSTA), RDSTA_ALL);
    assert_ops(ops, ARRAY_SIZE(ops));
    /* The TRNG can't be programmed under an instantiated state handle */
    assert_int_equal(model.kicks, 0);
}

/* Too little entropy: longer entropy delays until it works */
static void test_entropy_retry(void **state)
{
    static const enum op ops[] = { OP_INST0, OP_INST0, OP_INST0, OP_SECKEYS, OP_INST1 };

    (void)state;
    model.fail_inst = 2;
    assert_int_equal(imx_caam_rng_init((uintptr_t)regs), PB_OK);
    assert_int_equal(REG(RDSTA), RDSTA_ALL);
    assert_ops(ops, ARRAY_SIZE(ops));
    assert_int_equal(model.kicks, 3);
    assert_int_equal(model.ent_delay[0], 3200);
    assert_int_equal(model.ent_delay[1], 3400);
    assert_int_equal(model.ent_delay[2], 3600);
}

/* Never shorter than a delay already programmed, e.g. by the ROM */
static void test_entropy_delay_kept(void **state)
{
    (void)state;
    REG(RTSDCTL) = (4000 << 16) | 512;
    assert_int_equal(imx_caam_rng_init((uintptr_t)regs), PB_OK);
    assert_int_equal(model.kicks, 1);
    assert_int_equal(model.ent_delay[0], 4000);
    assert_int_equal(REG(RTFRQMIN), 4000 / 4);
}

static void test_entropy_gives_up(void **state)
{
    (void)state;
    model.fail_inst = 1000;
    assert_int_equal(imx_caam_rng_init((uintptr_t)regs), -PB_ERR);
    assert_int_equal(REG(RDSTA), 0);
    /* 3200 to 12800 in steps of 200 */
    assert_int_equal(model.jobs, 49);
    assert_int_equal(model.ent_delay[model.kicks - 1], 12800);
}

static void test_job_timeout(void **state)
{
    (void)state;
    model.hang = true;
    fake_us_tick_step = 1000;
    assert_int_equal(imx_caam_rng_init((uintptr_t)regs), -PB_ERR_TIMEOUT);
    assert_int_equal(model.kicks, 1);
}

static void test_one_state_handle(void **state)
{
    static const enum op ops[] = { OP_INST0, OP_SECKEYS };

    (void)state;
    REG(CTPR_MS) = 1 << 8;
    assert_int_equal(imx_caam_rng_init((uintptr_t)regs), PB_OK);
    assert_int_equal(REG(RDSTA), RDSTA_IF0 | RDSTA_PR0 | RDSTA_SKVN);
    assert_ops(ops, ARRAY_SIZE(ops));
}

static void test_no_rng4(void **state)
{
    (void)state;
    REG(CHAVID_LS) = (2 << 16) | (1 << 12);
    assert_int_equal(imx_caam_rng_init((uintptr_t)regs), PB_OK);
    assert_int_equal(model.jobs, 0);
    assert_int_equal(model.kicks, 0);
}

/* From era 10 the RNG's version is in its own register */
static void test_era10(void **state)
{
    (void)state;
    REG(CCBVID) = 10 << 24;
    REG(CHAVID_LS) = 0;
    REG(RNG_VERSION) = 4 << 24;
    assert_int_equal(imx_caam_rng_init((uintptr_t)regs), PB_OK);
    assert_int_equal(REG(RDSTA), RDSTA_ALL);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_not_initialized),
        cmocka_unit_test_setup(test_instantiate, setup),
        cmocka_unit_test_setup(test_already_instantiated, setup),
        cmocka_unit_test_setup(test_without_prediction_resistance, setup),
        cmocka_unit_test_setup(test_entropy_retry, setup),
        cmocka_unit_test_setup(test_entropy_delay_kept, setup),
        cmocka_unit_test_setup(test_entropy_gives_up, setup),
        cmocka_unit_test_setup(test_job_timeout, setup),
        cmocka_unit_test_setup(test_one_state_handle, setup),
        cmocka_unit_test_setup(test_no_rng4, setup),
        cmocka_unit_test_setup(test_era10, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
