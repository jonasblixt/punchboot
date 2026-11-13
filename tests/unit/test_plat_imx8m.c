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

/* The fuses */
static struct {
    uint32_t unique[2]; /* UNIQUE_ID's two words */
    unsigned int reads; /* imx_ocotp_read() calls */
    unsigned int fail_at; /* The call that fails, counted from 1; 0: none */
} fuses;

void imx_rom_log_save(const struct imx_rom_log_config *cfg)
{
    (void)cfg;
}

int imx_hab_log_init(const struct imx_hab_log_config *cfg)
{
    (void)cfg;
    return PB_OK;
}

int imx_ocotp_init(uintptr_t base, unsigned int words_per_bank, unsigned int ipg_rate_hz)
{
    (void)base;
    (void)words_per_bank;
    (void)ipg_rate_hz;
    return PB_OK;
}

/* A read that fails returns its error and leaves the value alone, as the
 * driver's does */
int imx_ocotp_read(uint32_t bank, uint32_t row, uint32_t *value)
{
    if (++fuses.reads == fuses.fail_at)
        return -PB_ERR_IO;

    if (bank == IMX8M_FUSE_UNIQUE_BANK && row == IMX8M_FUSE_UNIQUE1_WORD)
        *value = fuses.unique[0];
    else if (bank == IMX8M_FUSE_UNIQUE_BANK && row == IMX8M_FUSE_UNIQUE2_WORD)
        *value = fuses.unique[1];
    else
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
    memset(&fuses, 0, sizeof(fuses));
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

/* What plat_get_boot_reason() makes of a value of SRC_SRSR */
static struct pb_boot_reason reason_of(uint32_t srsr)
{
    struct pb_boot_reason reason;

    memset(&reason, 0xff, sizeof(reason));
    fake_regs_set(IMX8M_SRC_SRSR, srsr);
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_get_boot_reason(&reason), PB_OK);
    assert_int_equal(reason.raw, srsr);
    return reason;
}

/* platform.md 3.4: every cause of SRC_SRSR has a kind and a name, and raw is
 * the register (plat/imx8m/mm.h has the table). Bit 8 is the i.MX8M Plus's
 * WDOG2 */
static void test_boot_reason_kinds(void **state)
{
    static const struct {
        uint32_t srsr;
        enum pb_boot_reason_kind kind;
        const char *name;
    } causes[] = {
        { SRC_SRSR_IPP_RST_B, PB_BOOT_REASON_POR, "POR" },
        { SRC_SRSR_CSU_RST_B, PB_BOOT_REASON_FAULT, "CSU" },
        { SRC_SRSR_IPP_USER_RST_B, PB_BOOT_REASON_UNKNOWN, "User" },
        { SRC_SRSR_WDOG1_RST_B, PB_BOOT_REASON_WDOG, "wdog" },
        { SRC_SRSR_JTAG_RST_B, PB_BOOT_REASON_DEBUG, "jtag_rst" },
        { SRC_SRSR_JTAG_SW_RST, PB_BOOT_REASON_DEBUG, "jtag_sw" },
        { SRC_SRSR_WDOG3_RST_B, PB_BOOT_REASON_WDOG, "wdog3" },
        { SRC_SRSR_WDOG4_RST_B, PB_BOOT_REASON_WDOG, "wdog2" },
        { SRC_SRSR_TEMP_RST_B, PB_BOOT_REASON_FAULT, "temp" },
        /* Nothing set (the OS cleared it), a reserved bit */
        { 0, PB_BOOT_REASON_UNKNOWN, "Unknown" },
        { BIT(1), PB_BOOT_REASON_UNKNOWN, "Unknown" },
    };

    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(causes); i++) {
        struct pb_boot_reason reason = reason_of(causes[i].srsr);

        assert_int_equal(reason.kind, causes[i].kind);
        assert_string_equal(reason.name, causes[i].name);
    }
}

/* platform.md 3.4: the status is sticky, so several causes are set after the
 * resets that follow a power on. The priority of the platform header decides:
 * the highest bit, and a power on is the lowest */
static void test_boot_reason_priority(void **state)
{
    struct pb_boot_reason reason;
    (void)state;

    reason = reason_of(SRC_SRSR_IPP_RST_B | SRC_SRSR_WDOG1_RST_B);
    assert_int_equal(reason.kind, PB_BOOT_REASON_WDOG);
    assert_string_equal(reason.name, "wdog");

    reason = reason_of(SRC_SRSR_IPP_RST_B | SRC_SRSR_IPP_USER_RST_B);
    assert_int_equal(reason.kind, PB_BOOT_REASON_UNKNOWN);
    assert_string_equal(reason.name, "User");

    reason = reason_of(SRC_SRSR_IPP_RST_B | SRC_SRSR_WDOG1_RST_B | SRC_SRSR_JTAG_SW_RST);
    assert_int_equal(reason.kind, PB_BOOT_REASON_DEBUG);
    assert_string_equal(reason.name, "jtag_sw");

    reason = reason_of(0x3fd);
    assert_int_equal(reason.kind, PB_BOOT_REASON_FAULT);
    assert_string_equal(reason.name, "temp");
}

/* platform.md 2.1, 3.4: the reason is read in plat_early_init() and never
 * cleared: the OS reads the same register (D-053) */
static void test_boot_reason_is_read_once_and_not_cleared(void **state)
{
    struct pb_boot_reason reason;
    (void)state;

    reason = reason_of(SRC_SRSR_IPP_RST_B | SRC_SRSR_WDOG1_RST_B);
    assert_int_equal(plat_init(), PB_OK);

    /* What the register reads later doesn't matter */
    fake_regs_set(IMX8M_SRC_SRSR, SRC_SRSR_IPP_RST_B);
    assert_int_equal(plat_get_boot_reason(&reason), PB_OK);
    assert_int_equal(reason.kind, PB_BOOT_REASON_WDOG);
    assert_int_equal(reason.raw, SRC_SRSR_IPP_RST_B | SRC_SRSR_WDOG1_RST_B);

    assert_int_equal(fake_regs_writes(IMX8M_SRC_SRSR), 0);
}

/* platform.md 3.5, 3.7: the unique id is the two UNIQUE_ID words (bank 0,
 * words 2 and 3 on the i.MX8M Plus, F-169), the first word first, each with
 * its low byte first: eight bytes that never change, because the device's
 * UUID is made from them */
static void test_unique_id(void **state)
{
    static const uint8_t expected[8] = { 0x04, 0x03, 0x02, 0x01, 0x08, 0x07, 0x06, 0x05 };
    uint8_t id[16];
    size_t length = sizeof(id);
    (void)state;

    assert_int_equal(IMX8M_FUSE_UNIQUE_BANK, 0);
    assert_int_equal(IMX8M_FUSE_UNIQUE1_WORD, 2);
    assert_int_equal(IMX8M_FUSE_UNIQUE2_WORD, 3);

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

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_tick_from_early_init, setup),
        cmocka_unit_test_setup(test_tick_not_restarted_by_plat_init, setup),
        cmocka_unit_test_setup(test_boot_reason_kinds, setup),
        cmocka_unit_test_setup(test_boot_reason_priority, setup),
        cmocka_unit_test_setup(test_boot_reason_is_read_once_and_not_cleared, setup),
        cmocka_unit_test_setup(test_unique_id, setup),
        cmocka_unit_test_setup(test_unique_id_read_fails, setup),
        cmocka_unit_test_setup(test_unique_id_buffer_too_small, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
