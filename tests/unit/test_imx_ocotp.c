/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The OCOTP driver (src/drivers/fuse/imx_ocotp.c) against a model of the
 * controller behind the fake pb/mmio.h, as the 6UL RM (chapter 35) describes
 * it: CTRL with its SET/CLR/TOG aliases, BUSY for a while after a read or a
 * program, ERROR on an access to a locked word, cleared only through
 * CTRL_CLR (35.5.1). No fuse is ever near this test.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/fuse/imx_ocotp.h>
#include <pb/pb.h>
#include <string.h>

#include "fakes/plat.h"

#define BASE               0x021bc000u /* The 6UL's */
#define WORDS_PER_BANK     8
#define WORDS              128

#define OCOTP_CTRL_SET_    0x0004
#define OCOTP_CTRL_CLR_    0x0008
#define OCOTP_CTRL_TOG_    0x000c
#define OCOTP_SCS_CLR_     0x0068

#define CTRL_ADDR(ctrl)    ((ctrl) & 0x7f)
#define CTRL_UNLOCK(ctrl)  ((ctrl) >> 16)

/* CTRL reads that see BUSY after a read and after a program */
#define READ_BUSY_READS    2
#define PROGRAM_BUSY_READS 3

static struct {
    uint32_t ctrl; /* ADDR, ERROR and WR_UNLOCK. BUSY is busy_reads */
    uint32_t timing;
    uint32_t read_fuse_data;
    uint32_t sw_sticky;
    uint32_t scs;
    bool scs_set_ignored;
    uint32_t otp[WORDS];
    bool locked[WORDS];
    unsigned int busy_reads;
    bool programming; /* A program's BUSY hasn't been seen clear yet */
    bool postamble; /* It has, at postamble_tick */
    unsigned int postamble_tick;
    unsigned int violations; /* Accesses the RM forbids */
    unsigned int postamble_violations;
    unsigned int programs;
    uint32_t timing_at_program;
} m;

static void violation(const char *what)
{
    print_message("OCOTP model: %s\n", what);
    m.violations++;
}

uint64_t fake_mmio_read(uintptr_t addr, unsigned int width)
{
    assert_int_equal(width, 32);

    switch (addr - BASE) {
    case OCOTP_CTRL:
        if (m.busy_reads > 0) {
            m.busy_reads--;
            return m.ctrl | OCOTP_CTRL_BUSY;
        }

        if (m.programming) {
            m.programming = false;
            m.postamble = true;
            m.postamble_tick = fake_us_tick;
        }

        return m.ctrl;
    case OCOTP_TIMING:
        return m.timing;
    case OCOTP_READ_FUSE_DATA:
        return m.read_fuse_data;
    case OCOTP_SW_STICKY:
        return m.sw_sticky;
    case OCOTP_SCS:
        return m.scs;
    default:
        fail_msg("OCOTP model: read of 0x%" PRIxPTR, addr);
    }

    return 0;
}

/* An access that starts an OTP operation, or sets one up */
static void access(void)
{
    if (m.busy_reads > 0)
        violation("access while BUSY");

    /* 35.3.1.4: 2 us after BUSY cleared following a write */
    if (m.postamble) {
        if (fake_us_tick - m.postamble_tick < 2)
            m.postamble_violations++;

        m.postamble = false;
    }
}

void fake_mmio_write(uintptr_t addr, uint64_t value64, unsigned int width)
{
    uint32_t value = (uint32_t)value64;
    unsigned int word;

    assert_int_equal(width, 32);

    switch (addr - BASE) {
    case OCOTP_CTRL:
        access();
        /* A plain write leaves ERROR as it is (35.5.1) */
        m.ctrl = (value & ~(OCOTP_CTRL_BUSY | OCOTP_CTRL_ERROR)) | (m.ctrl & OCOTP_CTRL_ERROR);
        break;
    case OCOTP_CTRL_SET_:
        m.ctrl |= value & ~(OCOTP_CTRL_BUSY | OCOTP_CTRL_ERROR);
        break;
    case OCOTP_CTRL_CLR_:
        m.ctrl &= ~(value & ~OCOTP_CTRL_BUSY);
        break;
    case OCOTP_TIMING:
        access();
        m.timing = value;
        break;
    case OCOTP_DATA:
        access();
        word = CTRL_ADDR(m.ctrl);

        if (m.ctrl & OCOTP_CTRL_ERROR) {
            violation("program with ERROR set");
        } else if (CTRL_UNLOCK(m.ctrl) != OCOTP_CTRL_WR_KEY || m.locked[word]) {
            /* No access, no BUSY */
            m.ctrl |= OCOTP_CTRL_ERROR;
        } else {
            m.otp[word] |= value;
            m.programs++;
            m.timing_at_program = m.timing;
            m.busy_reads = PROGRAM_BUSY_READS;
            m.programming = true;
        }

        m.ctrl &= 0xffff; /* The unlock is used up */
        break;
    case OCOTP_READ_CTRL:
        access();
        word = CTRL_ADDR(m.ctrl);

        if (!(value & 1))
            break;

        if (m.ctrl & OCOTP_CTRL_ERROR) {
            violation("read with ERROR set");
        } else if (m.locked[word]) {
            m.ctrl |= OCOTP_CTRL_ERROR;
            m.read_fuse_data = 0xbadabada;
        } else {
            m.read_fuse_data = m.otp[word];
            m.busy_reads = READ_BUSY_READS;
        }
        break;
    case OCOTP_SW_STICKY:
        m.sw_sticky |= value;
        break;
    case OCOTP_SCS:
        m.scs = value;
        break;
    case OCOTP_SCS_SET:
        if (!m.scs_set_ignored)
            m.scs |= value;
        break;
    default:
        fail_msg("OCOTP model: write of 0x%" PRIx32 " to 0x%" PRIxPTR, value, addr);
    }
}

#define LOCKS (OCOTP_SW_STICKY_SRK_REVOKE_LOCK | OCOTP_SW_STICKY_FIELD_RETURN_LOCK)

static int setup(void **state)
{
    (void)state;
    memset(&m, 0, sizeof(m));
    fake_us_tick = FAKE_US_TICK_DEFAULT;
    fake_us_tick_step = 1;
    assert_int_equal(imx_ocotp_init(BASE, WORDS_PER_BANK), PB_OK);
    return 0;
}

static int teardown(void **state)
{
    (void)state;
    assert_int_equal(m.violations, 0);
    assert_int_equal(m.postamble_violations, 0);
    return 0;
}

/* Runs first: the driver keeps its base between tests */
static void test_not_initialized(void **state)
{
    (void)state;
    assert_int_equal(imx_ocotp_lock(LOCKS, true), -PB_ERR_STATE);
}

static void test_lock(void **state)
{
    (void)state;

    assert_int_equal(imx_ocotp_lock(LOCKS, true), PB_OK);
    assert_int_equal(m.sw_sticky, LOCKS);
    assert_true(m.scs & OCOTP_SCS_LOCK);

    /* Bits already set stay set */
    m.sw_sticky = BIT(4);
    assert_int_equal(imx_ocotp_lock(LOCKS, false), PB_OK);
    assert_int_equal(m.sw_sticky, BIT(4) | LOCKS);
}

/* SCS didn't lock: the write went to SCS_SET, SCS reads unlocked */
static void test_scs_not_locked(void **state)
{
    (void)state;

    m.scs_set_ignored = true;
    assert_int_equal(imx_ocotp_lock(LOCKS, true), -PB_ERR_IO);
    assert_int_equal(imx_ocotp_lock(LOCKS, false), PB_OK);
}

static void test_read_write(void **state)
{
    uint32_t val = 0;
    (void)state;

    m.otp[2 * WORDS_PER_BANK + 3] = 0x12345678;
    assert_int_equal(imx_ocotp_read(2, 3, &val), PB_OK);
    assert_int_equal(val, 0x12345678);

    assert_int_equal(imx_ocotp_write(5, 7, 0x00a5005a), PB_OK);
    assert_int_equal(m.otp[5 * WORDS_PER_BANK + 7], 0x00a5005a);
    assert_int_equal(m.programs, 1);

    /* By index */
    assert_int_equal(imx_ocotp_fuse_read(5 * WORDS_PER_BANK + 7, &val), PB_OK);
    assert_int_equal(val, 0x00a5005a);
}

/* F-197: ERROR is cleared through CTRL_CLR, so one failed access is one
 * error, not every access until a reset */
static void test_error_is_cleared(void **state)
{
    uint32_t val = 0;
    (void)state;

    m.otp[10] = 0xcafe;

    /* Left by someone before pb */
    m.ctrl |= OCOTP_CTRL_ERROR;
    assert_int_equal(imx_ocotp_read(1, 2, &val), -PB_ERR_IO);
    assert_int_equal(imx_ocotp_read(1, 2, &val), PB_OK);
    assert_int_equal(val, 0xcafe);

    /* A program of a locked word */
    m.locked[11] = true;
    assert_int_equal(imx_ocotp_write(1, 3, 1), -PB_ERR_IO);
    assert_int_equal(m.programs, 0);
    assert_int_equal(imx_ocotp_write(1, 4, 1), PB_OK);
    assert_int_equal(m.otp[12], 1);
    assert_int_equal(imx_ocotp_read(1, 2, &val), PB_OK);
    assert_int_equal(val, 0xcafe);
}

/* F-197: 2 us after a write's BUSY cleared before the next access
 * (35.3.1.4) */
static void test_write_postamble(void **state)
{
    uint32_t val = 0;
    (void)state;

    assert_int_equal(imx_ocotp_write(0, 5, 0x80), PB_OK);
    assert_int_equal(imx_ocotp_read(0, 5, &val), PB_OK);
    assert_int_equal(val, 0x80);
    assert_int_equal(imx_ocotp_write(0, 6, 0x81), PB_OK);
    assert_int_equal(imx_ocotp_write(0, 7, 0x82), PB_OK);
    assert_int_equal(m.postamble_violations, 0);
}

/* F-197: an index past CTRL's ADDR field is refused, not masked onto a word
 * at the start of the fuse box */
static void test_index_out_of_range(void **state)
{
    uint32_t val = 0x5555;
    (void)state;

    m.otp[0] = 0x11;
    assert_int_equal(imx_ocotp_read(WORDS / WORDS_PER_BANK, 0, &val), -PB_ERR_PARAM);
    assert_int_equal(imx_ocotp_fuse_read(WORDS, &val), -PB_ERR_PARAM);
    /* bank * words per bank wraps to 0 in 32 bits */
    assert_int_equal(imx_ocotp_read(0x20000000, 0, &val), -PB_ERR_PARAM);
    assert_int_equal(val, 0x5555);

    assert_int_equal(imx_ocotp_write(WORDS / WORDS_PER_BANK, 0, 0x22), -PB_ERR_PARAM);
    assert_int_equal(imx_ocotp_fuse_write(WORDS + 1, 0x22), -PB_ERR_PARAM);
    assert_int_equal(imx_ocotp_write(0x20000000, 0, 0x22), -PB_ERR_PARAM);
    assert_int_equal(m.programs, 0);
    assert_int_equal(m.otp[0], 0x11);
    assert_int_equal(m.otp[1], 0);

    /* The last word */
    assert_int_equal(imx_ocotp_fuse_read(WORDS - 1, &val), PB_OK);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_not_initialized),
        cmocka_unit_test_setup_teardown(test_lock, setup, teardown),
        cmocka_unit_test_setup_teardown(test_scs_not_locked, setup, teardown),
        cmocka_unit_test_setup_teardown(test_read_write, setup, teardown),
        cmocka_unit_test_setup_teardown(test_error_is_cleared, setup, teardown),
        cmocka_unit_test_setup_teardown(test_write_postamble, setup, teardown),
        cmocka_unit_test_setup_teardown(test_index_out_of_range, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
