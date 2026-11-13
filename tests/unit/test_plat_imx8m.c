/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M port (src/plat/imx8m/plat.c, built for the i.MX8M Plus) against
 * the platform contract (doc/interfaces/platform.md), with the SoC's
 * registers in a register file (fakes/regs.h). The GPT's and the WDOG's
 * drivers are the real ones; the fuses, the ROM's logs, the TZASC, the SoC's
 * clocks, the DRAM training and the MMU are fakes.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch_helpers.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/rom_log/imx_hab_log.h>
#include <drivers/rom_log/imx_rom_log.h>
#include <drivers/tzc/tzc380.h>
#include <drivers/wdog/imx_wdog.h>
#include <pb/console.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imx8m/dram.h>
#include <plat/imx8m/imx8m.h>
#include <string.h>

#include "fakes/regs.h"
#include "fakes/xlat.h"

#include "../../src/plat/imx8m/plat.h"

/* The GPT (8MP RM 12.1.5) */
#define GPT_CR     0x00
#define GPT_CR_EN  BIT(0)
#define GPT_CR_SWR BIT(15)
#define GPT_CNT    0x24

static struct {
    unsigned int log_time_starts;
    /* The register log's length when log_time_start() was called */
    size_t log_time_started_at;
} f;

void imx_rom_log_save(const struct imx_rom_log_config *cfg)
{
    (void)cfg;
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
    (void)row;
    *value = 0;
    return PB_OK;
}

int tzc380_init(uintptr_t base)
{
    (void)base;
    return PB_OK;
}

int tzc380_configure_region(unsigned int region, uint64_t region_base, uint64_t size, uint32_t sp)
{
    (void)region;
    (void)region_base;
    (void)size;
    (void)sp;
    return PB_OK;
}

void imx8m_soc_clock_init(void)
{
}

int imx8m_ddr_init(void)
{
    return PB_OK;
}

void write_cntfrq_el0(uint64_t value)
{
    (void)value;
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
    /* The GPT's software reset ends by itself */
    if (addr == IMX8M_GPT1_BASE + GPT_CR)
        return value & ~GPT_CR_SWR;

    return value;
}

static int setup(void **state)
{
    (void)state;
    memset(&f, 0, sizeof(f));
    fake_regs_reset();
    fake_regs_on_write = soc_on_write;
    fake_mmu_enables = 0;
    return 0;
}

/* platform.md 2.1, 3.2: when plat_early_init() returns the tick counts and
 * log_time_start() has been called, so that the board's console set up and
 * everything else before plat_init() can wait and has the time in its log
 * lines (F-238). GPT1's gate is opened before the GPT is written */
static void test_tick_from_early_init(void **state)
{
    size_t gate;
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);

    assert_true(fake_regs_get(IMX8M_GPT1_BASE + GPT_CR) & GPT_CR_EN);
    gate = fake_regs_first_write(CCM_CCGR_SET(CCGR_GPT1));
    assert_true(gate < fake_regs_first_write(IMX8M_GPT1_BASE + GPT_CR));

    /* The log's clock starts on a tick that counts */
    assert_int_equal(f.log_time_starts, 1);
    assert_int_equal(f.log_time_started_at, fake_reg_log_len);

    /* The tick is GPT1's counter, not a register at the driver's base of 0
     * (F-412) */
    fake_regs_set(IMX8M_GPT1_BASE + GPT_CNT, 123456);
    assert_int_equal(plat_get_us_tick(), 123456);
    assert_int_equal(fake_regs_reads(GPT_CNT), 0);
}

/* The tick isn't started again by plat_init(): a second start resets the
 * counter under the log's clock and under a timeout that is running */
static void test_tick_not_restarted_by_plat_init(void **state)
{
    size_t gpt_writes;
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    gpt_writes = fake_regs_writes_in(IMX8M_GPT1_BASE, 0x1000);

    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(fake_regs_writes_in(IMX8M_GPT1_BASE, 0x1000), gpt_writes);
    assert_int_equal(f.log_time_starts, 1);
    assert_int_equal(fake_mmu_enables, 1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_tick_from_early_init, setup),
        cmocka_unit_test_setup(test_tick_not_restarted_by_plat_init, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
