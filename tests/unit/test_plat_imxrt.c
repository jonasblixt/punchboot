/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX RT port (src/plat/imxrt/plat.c) against the platform contract
 * (doc/interfaces/platform.md), with the SoC's registers in a register file
 * (fakes/regs.h). The GPT's and the WDOG's drivers are the real ones; the
 * fuses, the HAB and Mbed TLS are fakes.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/crypto/mbedtls.h>
#include <drivers/fuse/imx_hab_fuses.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/rom_log/imx_hab_log.h>
#include <drivers/wdog/imx_wdog.h>
#include <pb/console.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imxrt/imxrt.h>
#include <string.h>

#include "fakes/regs.h"

/* The GPT (RM 52.6.1) */
#define GPT_CR                 0x00
#define GPT_CR_EN              BIT(0)
#define GPT_CR_ENMOD           BIT(1)
#define GPT_CR_CLKSRC(x)       ((x) << 6)
#define GPT_CR_FRR             BIT(9)
#define GPT_CR_EN_24M          BIT(10)
#define GPT_CR_SWR             BIT(15)
#define GPT_PR                 0x04
#define GPT_PR_PRESCALER(x)    ((x) << 0)
#define GPT_PR_PRESCALER24M(x) ((x) << 12)
#define GPT_CNT                0x24

/* CLKSRC: the 24 MHz crystal (ipg_clk_24M) */
#define GPT_CLKSRC_24M         5

static struct {
    int mbedtls_rc;
    uint32_t speed_grade;
    unsigned int log_time_starts;
    /* The register log's length when log_time_start() was called */
    size_t log_time_started_at;
} f;

/* The fuses */
static struct {
    uint32_t unique[2]; /* The unique id's two words */
    unsigned int reads; /* imx_ocotp_read() calls */
    unsigned int fail_at; /* The call that fails, counted from 1; 0: none */
} fuses;

/* A write that resets the SoC leaves the code under test through here */
static jmp_buf soc_reset;

int mbedtls_pb_init(void)
{
    return f.mbedtls_rc;
}

int imx_hab_log_init(const struct imx_hab_log_config *cfg)
{
    (void)cfg;
    return PB_OK;
}

int imx_ocotp_init(uintptr_t base, unsigned int words_per_bank)
{
    (void)base;
    (void)words_per_bank;
    return PB_OK;
}

/* A read that fails returns its error and leaves the value alone, as the
 * driver's does */
int imx_ocotp_read(uint32_t bank, uint32_t row, uint32_t *value)
{
    if (++fuses.reads == fuses.fail_at)
        return -PB_ERR_IO;

    if (bank == 0 && row == IMXRT_FUSE_UNIQUE_1)
        *value = fuses.unique[0];
    else if (bank == 0 && row == IMXRT_FUSE_UNIQUE_2)
        *value = fuses.unique[1];
    else if (bank == 0 && row == IMXRT_FUSE_SPEED_GRADE_WORD)
        *value = f.speed_grade << IMXRT_FUSE_SPEED_GRADE_SHIFT;
    else
        *value = 0;

    return PB_OK;
}

void imx_hab_fuses_init(const struct imx_hab_fuses *cfg)
{
    (void)cfg;
}

void log_time_start(void)
{
    f.log_time_starts++;
    f.log_time_started_at = fake_reg_log_len;
}

void log_prefix(const char *level, const char *func)
{
    (void)level;
    (void)func;
}

/* What the SoC does with a write */
static uint64_t soc_on_write(uintptr_t addr, uint64_t value)
{
    /* The ARM PLL locks at once */
    if (addr == IMXRT_CCM_ANALOG_PLL_ARM)
        return value | ANALOG_PLL_ARM_LOCK;

    /* The GPT's software reset ends by itself (RM 52.6.1.2) */
    if (addr == IMXRT_GPT1_BASE + GPT_CR)
        return value & ~GPT_CR_SWR;

    /* WDOG1's software reset: WCR.SRS written 0 (RM 57.8.1.2) */
    if (addr == IMXRT_WDOG1_BASE + WDOG_WCR && (value & WDOG_WCR_SRS) == 0)
        longjmp(soc_reset, 1);

    return value;
}

static int setup(void **state)
{
    (void)state;
    memset(&f, 0, sizeof(f));
    memset(&fuses, 0, sizeof(fuses));
    f.speed_grade = 2;
    fake_regs_reset();
    fake_regs_on_write = soc_on_write;
    return 0;
}

/* platform.md 2.2: plat_init() returns an error only when what command mode
 * needs is missing, and main() then resets. A crypto engine that failed
 * isn't that: PB_OK, and the boot has nothing to verify with */
static void test_plat_init_succeeds_without_the_crypto_engine(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    f.mbedtls_rc = -PB_ERR_MEM;
    assert_int_equal(plat_init(), PB_OK);
}

static void test_plat_init(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
}

/* platform.md 2.1, 3.2: when plat_early_init() returns the tick counts and
 * log_time_start() has been called (F-238). The tick's clock is the 24 MHz
 * crystal, divided by 8 and by 3 (RM 52.3.3, 52.6.1.2, 52.6.1.3): perclk,
 * the GPT's other source, changes its rate when plat_init() sets the ARM PLL */
static void test_tick_from_early_init(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);

    assert_int_equal(fake_regs_get(IMXRT_GPT1_BASE + GPT_CR),
                     GPT_CR_CLKSRC(GPT_CLKSRC_24M) | GPT_CR_EN_24M | GPT_CR_FRR | GPT_CR_ENMOD |
                         GPT_CR_EN);
    assert_int_equal(fake_regs_get(IMXRT_GPT1_BASE + GPT_PR),
                     GPT_PR_PRESCALER24M(8 - 1) | GPT_PR_PRESCALER(3 - 1));

    /* The log's clock starts on a tick that counts */
    assert_int_equal(f.log_time_starts, 1);
    assert_int_equal(f.log_time_started_at, fake_reg_log_len);

    /* The tick is GPT1's counter, not a register at the driver's base of 0
     * (F-412) */
    fake_regs_set(IMXRT_GPT1_BASE + GPT_CNT, 123456);
    assert_int_equal(plat_get_us_tick(), 123456);
    assert_int_equal(fake_regs_reads(GPT_CNT), 0);
}

/* plat_init() changes the core's clocks and leaves the tick alone: it isn't
 * started again, and no rate it is given depends on what plat_init() set */
static void test_tick_not_restarted_by_plat_init(void **state)
{
    size_t gpt_writes;
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    gpt_writes = fake_regs_writes_in(IMXRT_GPT1_BASE, 0x1000);

    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(fake_regs_writes_in(IMXRT_GPT1_BASE, 0x1000), gpt_writes);
    assert_int_equal(f.log_time_starts, 1);
}

/* What plat_get_boot_reason() makes of a value of WDOG1's WRSR */
static struct pb_boot_reason reason_of(uint16_t wrsr)
{
    struct pb_boot_reason reason;

    memset(&reason, 0xff, sizeof(reason));
    fake_regs_set(IMXRT_WDOG1_BASE + WDOG_WRSR, wrsr);
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_get_boot_reason(&reason), PB_OK);
    assert_int_equal(reason.raw, wrsr);
    return reason;
}

/* platform.md 3.4: every cause WRSR has gets a kind and a name, and raw is
 * the register (plat/imxrt/imxrt.h has the table). The port can tell the
 * watchdog's time-out from its software reset, which is a reboot */
static void test_boot_reason_kinds(void **state)
{
    struct pb_boot_reason reason;
    (void)state;

    reason = reason_of(WDOG_WRSR_POR);
    assert_int_equal(reason.kind, PB_BOOT_REASON_POR);
    assert_string_equal(reason.name, "POR");

    reason = reason_of(WDOG_WRSR_TOUT);
    assert_int_equal(reason.kind, PB_BOOT_REASON_WDOG);
    assert_string_equal(reason.name, "WDOG");

    reason = reason_of(WDOG_WRSR_SFTW);
    assert_int_equal(reason.kind, PB_BOOT_REASON_SW);
    assert_string_equal(reason.name, "SW");

    reason = reason_of(0);
    assert_int_equal(reason.kind, PB_BOOT_REASON_UNKNOWN);
    assert_string_equal(reason.name, "Unknown");

    reason = reason_of(BIT(8));
    assert_int_equal(reason.kind, PB_BOOT_REASON_UNKNOWN);
    assert_string_equal(reason.name, "Unknown");
}

/* platform.md 3.4: several causes set, which plat_boot_reason_str() called
 * "?" (F-236). The priority of the platform header decides: the time-out,
 * the software reset, the power on */
static void test_boot_reason_priority(void **state)
{
    struct pb_boot_reason reason;
    (void)state;

    reason = reason_of(WDOG_WRSR_POR | WDOG_WRSR_SFTW);
    assert_int_equal(reason.kind, PB_BOOT_REASON_SW);
    assert_string_equal(reason.name, "SW");

    reason = reason_of(WDOG_WRSR_POR | WDOG_WRSR_TOUT);
    assert_int_equal(reason.kind, PB_BOOT_REASON_WDOG);

    reason = reason_of(WDOG_WRSR_POR | WDOG_WRSR_SFTW | WDOG_WRSR_TOUT);
    assert_int_equal(reason.kind, PB_BOOT_REASON_WDOG);
    assert_string_equal(reason.name, "WDOG");
}

/* platform.md 2.1, 3.4: the reason is read in plat_early_init(), and
 * nothing is written to the register */
static void test_boot_reason_is_read_once_and_not_cleared(void **state)
{
    struct pb_boot_reason reason;
    (void)state;

    reason = reason_of(WDOG_WRSR_TOUT);
    assert_int_equal(plat_init(), PB_OK);

    /* What the register reads later doesn't matter */
    fake_regs_set(IMXRT_WDOG1_BASE + WDOG_WRSR, WDOG_WRSR_POR);
    assert_int_equal(plat_get_boot_reason(&reason), PB_OK);
    assert_int_equal(reason.kind, PB_BOOT_REASON_WDOG);
    assert_int_equal(reason.raw, WDOG_WRSR_TOUT);

    assert_int_equal(fake_regs_writes(IMXRT_WDOG1_BASE + WDOG_WRSR), 0);
}

/* platform.md 3.5, 3.7: the unique id is the two unique id words (bank 0,
 * words 1 and 2), the first word first, each with its low byte first: eight
 * bytes that never change, because the device's UUID is made from them */
static void test_unique_id(void **state)
{
    static const uint8_t expected[8] = { 0x04, 0x03, 0x02, 0x01, 0x08, 0x07, 0x06, 0x05 };
    uint8_t id[16];
    size_t length = sizeof(id);
    (void)state;

    assert_int_equal(IMXRT_FUSE_UNIQUE_1, 1);
    assert_int_equal(IMXRT_FUSE_UNIQUE_2, 2);

    fuses.unique[0] = 0x01020304;
    fuses.unique[1] = 0x05060708;
    memset(id, 0xa5, sizeof(id));

    assert_int_equal(plat_get_unique_id(id, &length), PB_OK);
    assert_int_equal(length, 8);
    assert_memory_equal(id, expected, sizeof(expected));
    assert_int_equal(id[8], 0xa5);
    assert_int_equal(fuses.reads, 2);
}

/* platform.md 3.5: a read that fails returns its error and writes nothing.
 * Never a partial id or one from memory that wasn't read (F-200: the error
 * was dropped and the id came from the stack) */
static void test_unique_id_read_fails(void **state)
{
    uint8_t untouched[16];
    uint8_t id[16];
    size_t length;
    (void)state;

    fuses.unique[0] = 0x01020304;
    fuses.unique[1] = 0x05060708;
    memset(untouched, 0xa5, sizeof(untouched));

    for (unsigned int fail_at = 1; fail_at <= 2; fail_at++) {
        memset(id, 0xa5, sizeof(id));
        length = sizeof(id);
        fuses.reads = 0;
        fuses.fail_at = fail_at;

        assert_int_equal(plat_get_unique_id(id, &length), -PB_ERR_IO);
        assert_memory_equal(id, untouched, sizeof(id));
        assert_int_equal(length, sizeof(id));
    }
}

/* platform.md 3.5: a buffer that is too small is its own error, and nothing
 * is read or written */
static void test_unique_id_buffer_too_small(void **state)
{
    uint8_t id[8] = { 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5 };
    size_t length = 7;
    (void)state;

    assert_int_equal(plat_get_unique_id(id, &length), -PB_ERR_BUF_TOO_SMALL);
    assert_int_equal(id[0], 0xa5);
    assert_int_equal(fuses.reads, 0);
}

/* platform.md 3.1: plat_reset() is declared noreturn. This function has no
 * return statement, which the compiler accepts only after a call that
 * doesn't return: without the attribute in pb/plat.h it doesn't build */
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wreturn-type"

static int reset_and_never_return(void)
{
    plat_reset();
}

#pragma GCC diagnostic pop

/* platform.md 3.1: plat_reset() resets the SoC, here through WDOG1's
 * software reset, and doesn't come back */
static void test_plat_reset(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);

    if (setjmp(soc_reset) == 0) {
        (void)reset_and_never_return();
        fail_msg("plat_reset() returned");
    }

    assert_int_equal(fake_reg_log[fake_reg_log_len - 1].addr, IMXRT_WDOG1_BASE + WDOG_WCR);
    assert_int_equal(fake_reg_log[fake_reg_log_len - 1].value, WDOG_WCR_SRE | WDOG_WCR_WDE);
}

/* platform.md 3.1: valid from the first instruction after arch_init(), also
 * before plat_early_init(). The reset needs nothing that the port sets up:
 * one write, to WDOG1, whose clock the ROM leaves on (plat-imx.md 3) */
static void test_plat_reset_before_early_init(void **state)
{
    (void)state;

    if (setjmp(soc_reset) == 0) {
        plat_reset();
        fail_msg("plat_reset() returned");
    }

    assert_int_equal(fake_reg_log_len, 1);
    assert_int_equal(fake_reg_log[0].addr, IMXRT_WDOG1_BASE + WDOG_WCR);
    assert_int_equal(fake_reg_log[0].value, WDOG_WCR_SRE | WDOG_WCR_WDE);
}

/* platform.md 3.6, 3.7: the platform's UUID namespace is declared in
 * pb/plat.h, and it never changes: the device's UUID and its tokens are made
 * from it. 48f8c6cb-8d07-4066-b921-0c1d8fd05471 */
static void test_platform_ns_uuid(void **state)
{
    static const uint8_t ns[16] = { 0x48, 0xf8, 0xc6, 0xcb, 0x8d, 0x07, 0x40, 0x66,
                                    0xb9, 0x21, 0x0c, 0x1d, 0x8f, 0xd0, 0x54, 0x71 };
    (void)state;

    assert_memory_equal(platform_ns_uuid, ns, sizeof(ns));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_plat_init, setup),
        cmocka_unit_test_setup(test_plat_init_succeeds_without_the_crypto_engine, setup),
        cmocka_unit_test_setup(test_tick_from_early_init, setup),
        cmocka_unit_test_setup(test_tick_not_restarted_by_plat_init, setup),
        cmocka_unit_test_setup(test_boot_reason_kinds, setup),
        cmocka_unit_test_setup(test_boot_reason_priority, setup),
        cmocka_unit_test_setup(test_boot_reason_is_read_once_and_not_cleared, setup),
        cmocka_unit_test_setup(test_unique_id, setup),
        cmocka_unit_test_setup(test_unique_id_read_fails, setup),
        cmocka_unit_test_setup(test_unique_id_buffer_too_small, setup),
        cmocka_unit_test_setup(test_plat_reset, setup),
        cmocka_unit_test_setup(test_plat_reset_before_early_init, setup),
        cmocka_unit_test_setup(test_platform_ns_uuid, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
