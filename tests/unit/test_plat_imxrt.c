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
#define GPT_CR     0x0
#define GPT_CR_SWR BIT(15)

static struct {
    int mbedtls_rc;
    uint32_t speed_grade;
} f;

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

int imx_ocotp_read(uint32_t bank, uint32_t row, uint32_t *value)
{
    (void)bank;

    if (row == IMXRT_FUSE_SPEED_GRADE_WORD)
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
        cmocka_unit_test_setup(test_plat_reset, setup),
        cmocka_unit_test_setup(test_plat_reset_before_early_init, setup),
        cmocka_unit_test_setup(test_platform_ns_uuid, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
