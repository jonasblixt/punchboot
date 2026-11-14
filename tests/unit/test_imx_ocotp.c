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
#define IPG_RATE           66000000u /* jiffy's ipg_clk */

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
    unsigned int program_busy_reads;
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
            m.busy_reads = m.program_busy_reads;
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
    m.program_busy_reads = PROGRAM_BUSY_READS;
    fake_us_tick_step = 1;
    assert_int_equal(imx_ocotp_init(BASE, WORDS_PER_BANK, IPG_RATE), PB_OK);
    return 0;
}

static int teardown(void **state)
{
    (void)state;
    assert_int_equal(m.violations, 0);
    assert_int_equal(m.postamble_violations, 0);
    return 0;
}

/* Runs first: the driver keeps its base between tests. F-420: a read or a
 * write before the init is -PB_ERR_STATE, where it went to address 0 */
static void test_not_initialized(void **state)
{
    uint32_t value = 0x5a5a5a5a;
    (void)state;
    assert_int_equal(imx_ocotp_lock(LOCKS, true), -PB_ERR_STATE);
    assert_int_equal(imx_ocotp_read(0, 1, &value), -PB_ERR_STATE);
    assert_int_equal(value, 0x5a5a5a5a);
    assert_int_equal(imx_ocotp_fuse_read(1, &value), -PB_ERR_STATE);
    assert_int_equal(imx_ocotp_write(0, 1, 1), -PB_ERR_STATE);
    assert_int_equal(imx_ocotp_fuse_write(1, 1), -PB_ERR_STATE);
}

/* F-420: an init that failed leaves the driver not set up, also after one
 * that worked: nothing reaches the controller */
static void test_a_failed_init_sets_up_nothing(void **state)
{
    uint32_t value;
    (void)state;

    assert_int_equal(imx_ocotp_init(BASE, 2, IPG_RATE), -PB_ERR_PARAM);
    assert_int_equal(imx_ocotp_read(0, 1, &value), -PB_ERR_STATE);
    assert_int_equal(imx_ocotp_write(0, 1, 1), -PB_ERR_STATE);
    assert_int_equal(imx_ocotp_lock(LOCKS, false), -PB_ERR_STATE);

    assert_int_equal(imx_ocotp_init(BASE, WORDS_PER_BANK, 2000000000u), -PB_ERR_PARAM);
    assert_int_equal(imx_ocotp_read(0, 1, &value), -PB_ERR_STATE);
    assert_int_equal(m.programs, 0);

    assert_int_equal(imx_ocotp_init(BASE, WORDS_PER_BANK, IPG_RATE), PB_OK);
    m.otp[1] = 0x1234;
    assert_int_equal(imx_ocotp_read(0, 1, &value), PB_OK);
    assert_int_equal(value, 0x1234);
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

/* F-197: a locked word reads 0xBADABADA with ERROR set: an error, not a
 * value */
static void test_read_of_a_locked_word(void **state)
{
    uint32_t val = 0x5555;
    (void)state;

    m.otp[20] = 0x1234;
    m.otp[21] = 0x4321;
    m.locked[20] = true;
    assert_int_equal(imx_ocotp_fuse_read(20, &val), -PB_ERR_IO);
    assert_int_equal(val, 0x5555);
    assert_int_equal(imx_ocotp_fuse_read(21, &val), PB_OK);
    assert_int_equal(val, 0x4321);
}

/* TIMING's fields (6UL RM 35.5.2), in ipg_clk periods */
#define T_STROBE_PROG(t) ((t) & 0xfff)
#define T_RELAX(t)       (((t) >> 12) & 0xf)
#define T_STROBE_READ(t) (((t) >> 16) & 0x3f)
#define T_WAIT(t)        (((t) >> 22) & 0x3f)

static uint32_t timing_at(unsigned int rate)
{
    assert_int_equal(imx_ocotp_init(BASE, WORDS_PER_BANK, rate), PB_OK);
    m.programs = 0;
    assert_int_equal(imx_ocotp_write(0, 1, 1), PB_OK);
    assert_int_equal(m.programs, 1);
    return m.timing_at_program;
}

/* F-196: at jiffy's 66 MHz TIMING is what pb always wrote, the reset value */
static void test_timing_66mhz(void **state)
{
    (void)state;
    assert_int_equal(timing_at(66000000), 0x01461299);
}

/* F-196: at any rate no time is shorter than at 66 MHz: the program strobe
 * tPGM = ((STROBE_PROG + 1) - 2 * (RELAX + 1)) periods is 10 us or more */
static void test_timing_scales_with_the_rate(void **state)
{
    static const unsigned int rates[] = {
        66000000, 66666666, 75000000, 132000000, 150000000, 200000000, 264000000,
    };
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(rates); i++) {
        uint32_t t = timing_at(rates[i]);
        uint64_t rate = rates[i];
        uint64_t relax = T_RELAX(t) + 1;
        uint64_t prog = T_STROBE_PROG(t) + 1 - 2 * relax;
        uint64_t read = T_STROBE_READ(t) + 1 - 2 * relax;

        print_message("%u Hz: TIMING 0x%08x\n", rates[i], t);
        assert_true(T_STROBE_PROG(t) + 1 > 2 * relax);
        assert_true(T_STROBE_READ(t) + 1 > 2 * relax);
        /* tPGM >= 10 us */
        assert_true(prog * 1000000 >= 10 * rate);
        /* As long as at 66 MHz: periods / rate >= periods_66 / 66 MHz */
        assert_true(prog * 66000000 >= 662 * rate);
        assert_true(relax * 66000000 >= 2 * rate);
        assert_true(read * 66000000 >= 3 * rate);
        assert_true((uint64_t)T_WAIT(t) * 66000000 >= 5 * rate);
    }
}

/* F-196: without the rate (0) the driver reads and refuses to program; a
 * rate whose timing doesn't fit the fields is refused */
static void test_timing_without_a_rate(void **state)
{
    uint32_t val = 0;
    (void)state;

    m.otp[3] = 0x77;
    assert_int_equal(imx_ocotp_init(BASE, WORDS_PER_BANK, 0), PB_OK);
    assert_int_equal(imx_ocotp_read(0, 3, &val), PB_OK);
    assert_int_equal(val, 0x77);
    assert_int_equal(imx_ocotp_write(0, 4, 1), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(m.programs, 0);
    assert_int_equal(m.timing, 0);

    /* A failed init leaves the driver not set up (F-420) */
    assert_int_equal(imx_ocotp_init(BASE, WORDS_PER_BANK, 1000000000), -PB_ERR_PARAM);
    assert_int_equal(imx_ocotp_write(0, 4, 1), -PB_ERR_STATE);
    assert_int_equal(m.programs, 0);
}

/* drivers.md 3: a BUSY that doesn't clear is -PB_ERR_TIMEOUT, and the
 * driver works once it clears */
static void test_busy_times_out(void **state)
{
    uint32_t val = 0;
    (void)state;

    m.otp[2] = 0x42;
    m.busy_reads = 1000000000;
    assert_int_equal(imx_ocotp_read(0, 2, &val), -PB_ERR_TIMEOUT);
    assert_int_equal(imx_ocotp_write(0, 2, 1), -PB_ERR_TIMEOUT);
    assert_int_equal(m.programs, 0);

    m.busy_reads = 0;
    assert_int_equal(imx_ocotp_read(0, 2, &val), PB_OK);
    assert_int_equal(val, 0x42);

    /* A program that doesn't end */
    m.program_busy_reads = 1000000000;
    assert_int_equal(imx_ocotp_write(0, 3, 1), -PB_ERR_TIMEOUT);
    assert_int_equal(m.programs, 1);
    m.busy_reads = 0;
    m.programming = false;
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_not_initialized),
        cmocka_unit_test_setup_teardown(test_a_failed_init_sets_up_nothing, setup, teardown),
        cmocka_unit_test_setup_teardown(test_lock, setup, teardown),
        cmocka_unit_test_setup_teardown(test_scs_not_locked, setup, teardown),
        cmocka_unit_test_setup_teardown(test_read_write, setup, teardown),
        cmocka_unit_test_setup_teardown(test_error_is_cleared, setup, teardown),
        cmocka_unit_test_setup_teardown(test_write_postamble, setup, teardown),
        cmocka_unit_test_setup_teardown(test_index_out_of_range, setup, teardown),
        cmocka_unit_test_setup_teardown(test_read_of_a_locked_word, setup, teardown),
        cmocka_unit_test_setup_teardown(test_timing_66mhz, setup, teardown),
        cmocka_unit_test_setup_teardown(test_timing_scales_with_the_rate, setup, teardown),
        cmocka_unit_test_setup_teardown(test_timing_without_a_rate, setup, teardown),
        cmocka_unit_test_setup_teardown(test_busy_times_out, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
