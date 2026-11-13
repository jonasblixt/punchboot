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

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_plat_init, setup),
        cmocka_unit_test_setup(test_plat_init_succeeds_without_the_crypto_engine, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
