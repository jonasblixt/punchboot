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
 * clocks, the DRAM training, the MMU and the crypto backends (the CAAM and
 * Mbed TLS: the test's build has both options) are fakes.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch_helpers.h>
#include <drivers/crypto/imx_caam.h>
#include <drivers/crypto/mbedtls.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/rom_log/imx_hab_log.h>
#include <drivers/rom_log/imx_rom_log.h>
#include <drivers/tzc/tzc380.h>
#include <drivers/wdog/imx_wdog.h>
#include <pb/console.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
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
    /* The ROM's logs and the TZASC's driver: what each answers */
    bool no_rom_log;
    /* The ids of the events in the ROM's log, 0 ends them */
    uint32_t rom_events[8];
    int hab_log_rc;
    int tzc_init_rc;
    int tzc_region_rc;
    /* GPR10's bits that don't take a write */
    uint32_t gpr10_stuck_at_0;
} f;

/* What plat_init() sets up after the clocks, in order */
static char calls[64];

/* A test that sets the platform up again and again fills it: what doesn't
 * fit is left out */
static void call(const char *name)
{
    if (strlen(calls) + strlen(name) + 2 > sizeof(calls))
        return;

    strcat(calls, name);
    strcat(calls, " ");
}

/* The crypto backends: what their set up returns, what it got, and the
 * register log's length and the MMU's state when the first was called */
static struct {
    int caam_rc, rng_rc, mbedtls_rc;
    uintptr_t caam_base, rng_base;
    size_t caam_at;
    unsigned int caam_mmu_enables;
} crypto;

int imx_caam_init(uintptr_t base)
{
    call("caam");
    crypto.caam_base = base;
    crypto.caam_at = fake_reg_log_len;
    crypto.caam_mmu_enables = fake_mmu_enables;
    return crypto.caam_rc;
}

int imx_caam_rng_init(uintptr_t ctrl_base)
{
    call("rng");
    crypto.rng_base = ctrl_base;
    return crypto.rng_rc;
}

int mbedtls_pb_init(void)
{
    call("mbedtls");
    return crypto.mbedtls_rc;
}

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

/* As the driver's: no log is -PB_ERR_NOT_FOUND, and a log that has no room
 * in 'out' is -PB_ERR_BUF_TOO_SMALL with nothing written */
int imx_rom_log_dump(char *out, size_t *out_len)
{
    (void)out;
    assert_int_equal(*out_len, 0);
    return f.no_rom_log ? -PB_ERR_NOT_FOUND : -PB_ERR_BUF_TOO_SMALL;
}

/* As the driver's: the event's id, which is bits 31:24 of its word in the
 * i.MX 8M's logs, and no event without a log */
bool imx_rom_log_has_event(uint32_t event)
{
    if (f.no_rom_log)
        return false;

    for (size_t i = 0; i < ARRAY_SIZE(f.rom_events) && f.rom_events[i] != 0; i++) {
        if (f.rom_events[i] == event)
            return true;
    }

    return false;
}

int imx_hab_log_init(const struct imx_hab_log_config *cfg)
{
    (void)cfg;
    return f.hab_log_rc;
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
    call("tzasc");
    return f.tzc_init_rc;
}

int tzc380_configure_region(unsigned int region, uint64_t region_base, uint64_t size, uint32_t sp)
{
    (void)region;
    (void)region_base;
    (void)size;
    (void)sp;
    return f.tzc_region_rc;
}

void imx8m_soc_clock_init(void)
{
}

int imx8m_ddr_init(void)
{
    call("ddr");
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

    if (addr == IMX8M_IOMUXC_GPR10)
        return value & ~(uint64_t)f.gpr10_stuck_at_0;

    return value;
}

static int setup(void **state)
{
    (void)state;
    memset(&f, 0, sizeof(f));
    memset(&fuses, 0, sizeof(fuses));
    memset(&crypto, 0, sizeof(crypto));
    memset(calls, 0, sizeof(calls));
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

/* What plat_get_status() answers, every field set by the port */
static struct plat_status status_now(void)
{
    struct plat_status status;

    memset(&status, 0xff, sizeof(status));
    plat_get_status(&status);
    return status;
}

/* platform.md 3.8: what plat_init() recorded. The TZASC is enabled and
 * locked and DRAM is open in region 1, the watchdog is armed (the test is
 * built with the option), the ROM's logs were found, and the crypto backends
 * the platform set up work */
static void test_status(void **state)
{
    struct plat_status status;
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    status = status_now();

    assert_int_equal(status.tz_dram_filter, PLAT_TZ_DRAM_ACTIVE);
    assert_true(status.wdog_armed);
    assert_true(status.crypto_engine);
    assert_true(status.rom_log);
}

/* platform.md 2.2, 4.3 (F-248): a TZASC whose enable or whose lock didn't
 * take isn't plat_init()'s error, command mode runs without it. It is
 * recorded as bypassed: the normal world can switch off what isn't locked */
static void test_status_tzasc_not_enabled(void **state)
{
    static const uint32_t bits[] = {
        GPR10_TZASC_EN,
        GPR10_TZASC_ID_SWAP_BYPASS,
        GPR10_LOCK_TZASC_EN,
        GPR10_LOCK_TZASC_ID_SWAP,
    };
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(bits); i++) {
        f.gpr10_stuck_at_0 = bits[i];
        fake_regs_set(IMX8M_IOMUXC_GPR10, 0);
        assert_int_equal(plat_early_init(), PB_OK);
        assert_int_equal(plat_init(), PB_OK);
        assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_BYPASSED);
    }

    /* And it is the state of this plat_init(), not of an earlier one */
    f.gpr10_stuck_at_0 = 0;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_ACTIVE);
}

/* The same when DRAM's region couldn't be opened with the TZASC enabled:
 * not the state 4.3 asks for, and the boot is refused the same way */
static void test_status_tzasc_setup_failed(void **state)
{
    (void)state;

    f.tzc_init_rc = -PB_ERR_NOT_SUPPORTED;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_BYPASSED);

    f.tzc_init_rc = PB_OK;
    f.tzc_region_rc = -PB_ERR_STATE;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_BYPASSED);
}

/* platform.md 2.3: the ROM's logs are two, its event log and the HAB's.
 * Without either the platform goes on and records it */
static void test_status_rom_logs_missing(void **state)
{
    (void)state;

    f.no_rom_log = true;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_false(status_now().rom_log);

    f.no_rom_log = false;
    f.hab_log_rc = -PB_ERR_NOT_FOUND;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_false(status_now().rom_log);
}

/* What plat_get_boot_source() answers after a plat_early_init() */
static enum pb_boot_source_kind boot_source_now(void)
{
    struct pb_boot_source src;

    memset(&src, 0xff, sizeof(src));
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_get_boot_source(&src), PB_OK);
    return src.kind;
}

/* platform.md 3.9 (F-251): the ROM's log has "Serial download", event 0xd0
 * of AN12853's Table 9, when its serial downloader loaded pb, which is over
 * USB on the i.MX 8M */
static void test_boot_source_serial_download(void **state)
{
    static const uint32_t events[] = { 0xe0, 0x22, 0x31, 0x40, 0xd0, 0xa0, 0xc0 };
    (void)state;

    memcpy(f.rom_events, events, sizeof(events));
    assert_int_equal(boot_source_now(), PB_BOOT_SOURCE_USB);
}

/* A boot from the boot device, with the events of the 8MP EVK's log of a
 * boot from its eMMC (test_imx_rom_log.c): no serial download, and the
 * partition isn't named. Unknown, which boots. The same without a log */
static void test_boot_source_not_a_download(void **state)
{
    static const uint32_t events[] = { 0xe0, 0x22, 0x31, 0x40, 0x61, 0x82, 0x83, 0x80 };
    (void)state;

    memcpy(f.rom_events, events, sizeof(events));
    assert_int_equal(boot_source_now(), PB_BOOT_SOURCE_UNKNOWN);

    static const uint32_t more[] = { 0x81, 0x50, 0x90, 0x91, 0xa0, 0xc0 };

    memset(f.rom_events, 0, sizeof(f.rom_events));
    memcpy(f.rom_events, more, sizeof(more));
    assert_int_equal(boot_source_now(), PB_BOOT_SOURCE_UNKNOWN);

    f.no_rom_log = true;
    f.rom_events[0] = 0xd0;
    assert_int_equal(boot_source_now(), PB_BOOT_SOURCE_UNKNOWN);
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

/* platform.md 2.2, crypto.md 2 (F-247): plat_init() sets up the build's
 * crypto backends, each behind its driver's option: the CAAM on job ring 0,
 * then its RNG4 on the controller page, and Mbed TLS.
 *
 * Until F-247 the boards' init() did the CAAM's two calls as its first step,
 * which was the first thing after plat_init(): they are plat_init()'s last
 * now, after the DRAM's training and the TZASC and with the MMU on, and no
 * register of the platform's is written after them. So the SoC sees the same
 * writes in the same order */
static void test_crypto_set_up_last_in_plat_init(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_string_equal(calls, "");

    assert_int_equal(plat_init(), PB_OK);
    assert_string_equal(calls, "ddr tzasc caam rng mbedtls ");
    assert_int_equal(crypto.caam_base, IMX8M_CAAM_JR0_BASE);
    assert_int_equal(crypto.rng_base, IMX8M_CAAM_BASE);
    assert_int_equal(crypto.caam_mmu_enables, 1);
    assert_int_equal(crypto.caam_at, fake_reg_log_len);
}

/* platform.md 2.2: a crypto engine that fails isn't plat_init()'s error:
 * command mode can start without it, and main() would reset. The RNG isn't
 * instantiated on an engine that didn't come up; a failed RNG alone is no
 * reason not to boot (Linux has no hardware RNG then) */
static void test_plat_init_succeeds_without_the_crypto_engine(void **state)
{
    (void)state;

    crypto.caam_rc = -PB_ERR_IO;
    crypto.mbedtls_rc = -PB_ERR_PARAM;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_string_equal(calls, "ddr tzasc caam mbedtls ");

    setup(NULL);
    crypto.rng_rc = -PB_ERR_TIMEOUT;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_string_equal(calls, "ddr tzasc caam rng mbedtls ");
}

/* platform.md 2.3, 3.8 (F-247, F-248): the platform sets the build's crypto
 * backends up, so their result is the platform's status, and the boot's
 * preconditions refuse the boot when it is false, before a candidate is
 * tried (test_boot.c). A failed RNG
 * doesn't make it false */
static void test_status_crypto_engine(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_true(status_now().crypto_engine);

    /* The CAAM, which the boards verify with */
    setup(NULL);
    crypto.caam_rc = -PB_ERR_IO;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_false(status_now().crypto_engine);

    /* Each algorithm has one backend (crypto.md 2): a build with two needs
     * both */
    setup(NULL);
    crypto.mbedtls_rc = -PB_ERR_MEM;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_false(status_now().crypto_engine);

    /* The RNG is Linux's loss, not the boot's: the engine verifies */
    setup(NULL);
    crypto.rng_rc = -PB_ERR_TIMEOUT;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_true(status_now().crypto_engine);

    /* It is the state of this plat_init(), not of an earlier one */
    setup(NULL);
    assert_int_equal(plat_init(), PB_OK);
    assert_true(status_now().crypto_engine);
}

/* platform.md 4.1, 4.2 (F-247): the root of trust and the life cycle are the
 * platform's. pb doesn't support the 8M's HAB: what the 8MP EVK wired itself
 * until then, unchanged. Every key can be used and none is revoked; there is
 * no life cycle (command mode is open) and no fuse is written */
static void test_plat_rot_and_slc(void **state)
{
    static const struct slc_ops none;
    (void)state;

    assert_null(plat_rot.revoke_key);
    assert_non_null(plat_rot.read_key_status);

    for (unsigned int slot = 0; slot < 8; slot++) {
        const struct rot_key key = { .id = 0xa90f9680, .slot = slot };

        assert_int_equal(plat_rot.read_key_status(&key), PB_OK);
    }

    assert_memory_equal(&plat_slc, &none, sizeof(none));
    assert_int_equal(fake_reg_log_len, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_tick_from_early_init, setup),
        cmocka_unit_test_setup(test_tick_not_restarted_by_plat_init, setup),
        cmocka_unit_test_setup(test_status, setup),
        cmocka_unit_test_setup(test_status_tzasc_not_enabled, setup),
        cmocka_unit_test_setup(test_status_tzasc_setup_failed, setup),
        cmocka_unit_test_setup(test_status_rom_logs_missing, setup),
        cmocka_unit_test_setup(test_boot_source_serial_download, setup),
        cmocka_unit_test_setup(test_boot_source_not_a_download, setup),
        cmocka_unit_test_setup(test_boot_reason_kinds, setup),
        cmocka_unit_test_setup(test_boot_reason_priority, setup),
        cmocka_unit_test_setup(test_boot_reason_is_read_once_and_not_cleared, setup),
        cmocka_unit_test_setup(test_unique_id, setup),
        cmocka_unit_test_setup(test_unique_id_read_fails, setup),
        cmocka_unit_test_setup(test_unique_id_buffer_too_small, setup),
        cmocka_unit_test_setup(test_plat_rot_and_slc, setup),
        cmocka_unit_test_setup(test_crypto_set_up_last_in_plat_init, setup),
        cmocka_unit_test_setup(test_plat_init_succeeds_without_the_crypto_engine, setup),
        cmocka_unit_test_setup(test_status_crypto_engine, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
