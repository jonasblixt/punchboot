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
#include <drivers/fuse/imx_hab_fuses.h>
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

#include <xlat_tables.h>

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
    /* HAB's verdict on this boot (imx_hab_no_failure()), and the locks */
    int hab_no_failure_rc;
    unsigned int hab_locks;
    int tzc_init_rc;
    /* Region 1 as the TZASC has it, what the SPL set up (D-088) */
    int tzc_region_rc;
    uint64_t tzc_region_base, tzc_region_size;
    uint32_t tzc_region_sp;
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
    int caam_rc, rng_rc, mbedtls_rc, stop_rc, lock_rc;
    uint32_t lock_sticky; /* What imx_ocotp_lock() was asked to lock */
    bool lock_scs;
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

int imx_caam_stop(void)
{
    call("stop");
    return crypto.stop_rc;
}

/* The unique id's words: what the port asked the OCOTP's driver for, and its
 * answer. The driver makes the id of them (test_imx_ocotp.c) */
static struct {
    unsigned int calls;
    uint32_t bank, row1, row2;
    uint8_t *output;
    size_t *length;
    int rc;
} uid;

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

int imx_hab_no_failure(void)
{
    return f.hab_no_failure_rc;
}

void imx_hab_fuses_init(const struct imx_hab_fuses *cfg)
{
    (void)cfg;
}

/* The HAB's fuses and the OCOTP's access by index: the port only names them, in plat_rot and
 * plat_slc */
slc_t imx_hab_slc_read_status(void)
{
    return SLC_NOT_CONFIGURED;
}

int imx_hab_slc_set_configuration(void)
{
    return PB_OK;
}

int imx_hab_slc_set_configuration_locked(void)
{
    f.hab_locks++;
    return PB_OK;
}

int imx_ocotp_fuse_read(uint32_t index, uint32_t *value)
{
    (void)index;
    *value = 0;
    return PB_OK;
}

int imx_ocotp_fuse_write(uint32_t index, uint32_t value)
{
    (void)index;
    (void)value;
    return PB_OK;
}

int imx_ocotp_lock(uint32_t sw_sticky, bool lock_scs)
{
    crypto.lock_sticky = sw_sticky;
    crypto.lock_scs = lock_scs;
    call("lock");
    return crypto.lock_rc;
}

/* The ipg_clk rate plat_init() gave the OCOTP, for its fuse timing */
static unsigned int ocotp_ipg_hz;

int imx_ocotp_init(uintptr_t base, unsigned int words_per_bank, unsigned int ipg_rate_hz)
{
    (void)base;
    (void)words_per_bank;
    ocotp_ipg_hz = ipg_rate_hz;
    return PB_OK;
}

int imx_ocotp_read_unique_id(uint32_t bank,
                             uint32_t row1,
                             uint32_t row2,
                             uint8_t *output,
                             size_t *length)
{
    uid.calls++;
    uid.bank = bank;
    uid.row1 = row1;
    uid.row2 = row2;
    uid.output = output;
    uid.length = length;
    return uid.rc;
}

int tzc380_init(uintptr_t base)
{
    (void)base;
    call("tzasc");
    return f.tzc_init_rc;
}

int tzc380_get_region(unsigned int region, uint64_t *region_base, uint64_t *size, uint32_t *sp)
{
    assert_int_equal(region, 1);
    *region_base = f.tzc_region_base;
    *size = f.tzc_region_size;
    *sp = f.tzc_region_sp;
    return f.tzc_region_rc;
}

/* The SPL's (spl/spl.c, tzasc.c): pb proper never sets the TZASC up */
int tzc380_configure_region(unsigned int region, uint64_t region_base, uint64_t size, uint32_t sp)
{
    (void)region;
    (void)region_base;
    (void)size;
    (void)sp;
    fail_msg("pb proper configured a TZASC region");
    return -PB_ERR;
}

/* The variant's part (plat_imx8mp.c): the 8MP EVK's values */
static uint32_t fake_digprog = 0x00824310;
static const unsigned int fake_mts[] = { 4000 };
struct dram_rates dram_rates = { .mts = fake_mts, .num = 1 };

const char *imx8m_soc_name(void)
{
    return "i.MX8MP";
}

uint32_t imx8m_soc_digprog(void)
{
    return fake_digprog;
}

uint32_t imx8m_soc_cpu_hz(void)
{
    return 1000000000;
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
    memset(&uid, 0, sizeof(uid));
    memset(&crypto, 0, sizeof(crypto));
    memset(calls, 0, sizeof(calls));
    fake_regs_reset();
    fake_regs_on_write = soc_on_write;
    fake_mmu_enables = 0;
    /* What the SPL left (D-088): the TZASC enabled and locked, DRAM open to
     * both worlds in region 1 from the window's start */
    fake_regs_set(IMX8M_IOMUXC_GPR10,
                  GPR10_TZASC_EN | GPR10_TZASC_ID_SWAP_BYPASS | GPR10_LOCK_TZASC_EN |
                      GPR10_LOCK_TZASC_ID_SWAP);
    f.tzc_region_size = UINT64_C(8) << 30;
    f.tzc_region_sp = TZC380_SP_ALL;
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

/* platform.md 2.2, 4.3 (F-248, D-088): pb proper reads the TZASC back. One
 * whose enable or whose lock the SPL left off isn't plat_init()'s error,
 * command mode runs without it. It is recorded as bypassed: the normal
 * world can switch off what isn't locked */
static void test_status_tzasc_not_enabled(void **state)
{
    static const uint32_t bits[] = {
        GPR10_TZASC_EN,
        GPR10_TZASC_ID_SWAP_BYPASS,
        GPR10_LOCK_TZASC_EN,
        GPR10_LOCK_TZASC_ID_SWAP,
    };
    const uint32_t all = GPR10_TZASC_EN | GPR10_TZASC_ID_SWAP_BYPASS | GPR10_LOCK_TZASC_EN |
                         GPR10_LOCK_TZASC_ID_SWAP;
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(bits); i++) {
        fake_regs_set(IMX8M_IOMUXC_GPR10, all & ~bits[i]);
        assert_int_equal(plat_early_init(), PB_OK);
        assert_int_equal(plat_init(), PB_OK);
        assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_BYPASSED);
    }

    /* And it is the state of this plat_init(), not of an earlier one */
    fake_regs_set(IMX8M_IOMUXC_GPR10, all);
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_ACTIVE);
}

/* The same when region 1 isn't what the SPL is to set up: the TZC-380 not
 * there, its region not read, not from the window's start, smaller than
 * the board's DRAM, or closed to a world. Not the state 4.3 asks for, and
 * the boot is refused the same way */
static void test_status_tzasc_region_not_open(void **state)
{
    (void)state;

    f.tzc_init_rc = -PB_ERR_NOT_SUPPORTED;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_BYPASSED);

    setup(NULL);
    f.tzc_region_rc = -PB_ERR_STATE;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_BYPASSED);

    setup(NULL);
    f.tzc_region_base = SZ_GiB(1);
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_BYPASSED);

    /* The board's DRAM is 1 GiB from the window's start (CMakeLists.txt) */
    setup(NULL);
    f.tzc_region_size = SZ_GiB(1) / 2;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_BYPASSED);

    setup(NULL);
    f.tzc_region_size = SZ_GiB(1);
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_ACTIVE);

    setup(NULL);
    f.tzc_region_sp = TZC380_SP_ALL & ~TZC380_SP_NS_W;
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

/* platform.md 3.5, 3.7: the unique id is the two UNIQUE_ID words (bank 0, words 2 and 3 on the
 * i.MX8M Plus, F-169), the first word first: the OCOTP's driver reads them and makes the eight
 * bytes (test_imx_ocotp.c), which never change, because the device's UUID is
 * made from them */
static void test_unique_id(void **state)
{
    uint8_t id[16];
    size_t length = sizeof(id);
    (void)state;

    assert_int_equal(plat_get_unique_id(id, &length), PB_OK);
    assert_int_equal(uid.calls, 1);
    assert_int_equal(uid.bank, 0);
    assert_int_equal(uid.row1, 2);
    assert_int_equal(uid.row2, 3);
    assert_ptr_equal(uid.output, id);
    assert_ptr_equal(uid.length, &length);
}

/* platform.md 3.5: the driver's error is the port's (F-200) */
static void test_unique_id_read_fails(void **state)
{
    uint8_t id[16];
    size_t length = sizeof(id);
    (void)state;

    uid.rc = -PB_ERR_IO;
    assert_int_equal(plat_get_unique_id(id, &length), -PB_ERR_IO);
    uid.rc = -PB_ERR_BUF_TOO_SMALL;
    assert_int_equal(plat_get_unique_id(id, &length), -PB_ERR_BUF_TOO_SMALL);
}

/* platform.md 2.2, crypto.md 2 (F-247): plat_init() sets up the build's
 * crypto backends, each behind its driver's option: the CAAM on job ring 0,
 * then its RNG4 on the controller page, and Mbed TLS.
 *
 * Until F-247 the boards' init() did the CAAM's two calls as its first step,
 * which was the first thing after plat_init(): they are plat_init()'s last
 * now, after the TZASC's check (the SPL trained DRAM, D-088) and with the
 * MMU on, and no
 * register of the platform's is written after them. So the SoC sees the same
 * writes in the same order */
static void test_crypto_set_up_last_in_plat_init(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_string_equal(calls, "");

    assert_int_equal(plat_init(), PB_OK);
    assert_string_equal(calls, "tzasc caam rng mbedtls ");
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
    assert_string_equal(calls, "tzasc caam mbedtls ");

    setup(NULL);
    crypto.rng_rc = -PB_ERR_TIMEOUT;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_string_equal(calls, "tzasc caam rng mbedtls ");
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

/* platform.md 4.5, drivers.md 2.3, crypto.md 6 (F-286): the platform stops
 * what plat_init() started before the jump, in plat_lockdown(), the core's
 * last call of it: the CAAM's job ring. A stop that fails is a failed boot */
static void test_lockdown_stops_the_crypto_engine(void **state)
{
    (void)state;

    /* D-084: SRK revocation and field return locked in SW_STICKY, and SCS,
     * as on the 6UL, then the CAAM stopped */
    assert_int_equal(plat_lockdown(), PB_OK);
    assert_string_equal(calls, "lock stop ");
    assert_int_equal(crypto.lock_sticky,
                     OCOTP_SW_STICKY_SRK_REVOKE_LOCK | OCOTP_SW_STICKY_FIELD_RETURN_LOCK);
    assert_true(crypto.lock_scs);
    assert_int_equal(fake_reg_log_len, 0);

    /* A second lockdown before a reset (platform.md 4.4) */
    assert_int_equal(plat_lockdown(), PB_OK);
    assert_string_equal(calls, "lock stop lock stop ");

    /* A lock that didn't hold is a failed lockdown: the CAAM stays up for
     * command mode */
    setup(NULL);
    crypto.lock_rc = -PB_ERR_IO;
    assert_int_equal(plat_lockdown(), -PB_ERR_IO);
    assert_string_equal(calls, "lock ");

    setup(NULL);
    crypto.stop_rc = -PB_ERR_TIMEOUT;
    assert_int_equal(plat_lockdown(), -PB_ERR_TIMEOUT);
    assert_string_equal(calls, "lock stop ");
}

/* platform.md 4.1, 4.2: the root of trust and the life cycle are the
 * platform's, HABv4's fuses (fusebox.h). Nothing is read or written when
 * they are named */
static void test_plat_rot_and_slc(void **state)
{
    (void)state;

    /* The 8M Plus (this test's SoC): its SRK_REVOKE isn't public, no key is
     * revoked and every key reads as active */
    assert_null(plat_rot.revoke_key);
    assert_non_null(plat_rot.read_key_status);
    assert_int_equal(plat_rot.revoke_slots, 3);

    for (unsigned int slot = 0; slot < 8; slot++) {
        const struct rot_key key = { .id = 0xa90f9680, .slot = slot };

        assert_int_equal(plat_rot.read_key_status(&key), PB_OK);
    }

    /* The life cycle: HABv4's SEC_CONFIG and SRK hash, the fuses by index,
     * no EOL (F-263) */
    assert_ptr_equal(plat_slc.read_status, imx_hab_slc_read_status);
    assert_ptr_equal(plat_slc.set_configuration, imx_hab_slc_set_configuration);
    assert_non_null(plat_slc.set_configuration_locked);
    assert_null(plat_slc.set_eol);
    assert_ptr_equal(plat_slc.fuse_read, imx_ocotp_fuse_read);
    assert_ptr_equal(plat_slc.fuse_write, imx_ocotp_fuse_write);
    assert_int_equal(fake_reg_log_len, 0);
}

/* The OCOTP's fuse timing is for the ipg_clk the ROM left (CCM roots 32,
 * AHB, and 33, IPG): the pico8ml's ROM, SYSTEM_PLL1_DIV6 and IPG at AHB / 2.
 * A source that isn't a fixed rate: no rate, no fuse write (pb's own fuse
 * writes failed without one, on every 8M) */
static void test_ocotp_ipg_rate(void **state)
{
    (void)state;

    fake_regs_set(0x30388000 + 0x80 * 32, 0x11000000);
    fake_regs_set(0x30388000 + 0x80 * 33, 0x00000001);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(ocotp_ipg_hz, 66666666);

    setup(NULL);
    fake_regs_set(0x30388000 + 0x80 * 32, 0x14000000); /* SYSTEM_PLL2_DIV8 */
    fake_regs_set(0x30388000 + 0x80 * 33, 0x00000000);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(ocotp_ipg_hz, 125000000);

    setup(NULL);
    fake_regs_set(0x30388000 + 0x80 * 32, 0x15000000); /* SYSTEM_PLL3: not fixed */
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(ocotp_ipg_hz, 0);
}

/* Closing a device whose image HAB doesn't accept leaves one that doesn't
 * boot: the lock is refused on a HAB failure, and nothing is fused */
static void test_lock_waits_for_hab(void **state)
{
    (void)state;

    f.hab_no_failure_rc = -PB_ERR_STATE;
    assert_int_equal(plat_slc.set_configuration_locked(), -PB_ERR_STATE);
    assert_int_equal(f.hab_locks, 0);

    f.hab_no_failure_rc = -PB_ERR_NOT_FOUND;
    assert_int_equal(plat_slc.set_configuration_locked(), -PB_ERR_NOT_FOUND);
    assert_int_equal(f.hab_locks, 0);

    f.hab_no_failure_rc = PB_OK;
    assert_int_equal(plat_slc.set_configuration_locked(), PB_OK);
    assert_int_equal(f.hab_locks, 1);
}

/* platform.md 3.11: the variant's name and clock, DIGPROG's MINOR as the
 * revision, the DRAM's clock half the trained rate */
static void test_get_info(void **state)
{
    struct pb_plat_info info;
    char extra[32];
    (void)state;

    memset(extra, 0xa5, sizeof(extra));
    assert_int_equal(plat_get_info(&info, extra, sizeof(extra)), PB_OK);
    assert_string_equal(info.soc, "i.MX8MP");
    assert_string_equal(info.rev, "1.0");
    assert_int_equal(info.cpu_hz, 1000000000);
    assert_int_equal(info.mem_hz, 2000000000);
    assert_string_equal(extra, "");

    fake_digprog = 0x00824311; /* A1 */
    assert_int_equal(plat_get_info(&info, extra, sizeof(extra)), PB_OK);
    assert_string_equal(info.rev, "1.1");
    fake_digprog = 0x00824310;

    assert_int_equal(plat_get_info(&info, extra, 0), -PB_ERR_BUF_TOO_SMALL);
}

/* platform.md 5.2: the memory map. The rest of pb (.bss, the RAM log) and
 * the buffers are two regions, RW and execute never, and nothing maps the
 * address space between them: one span from pb to the buffers mapped it as
 * memory, on the i.MX8M Plus 1 GiB with device windows in it (F-158). The
 * board's DRAM is one region, the same, which pb's regions in it nest in */
static void test_mmu_map(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(fake_mmu_enables, 1);
    fake_mmu_check_pb_map(BOARD_RAM_BASE, BOARD_RAM_END);
}

/* platform.md 5.2: the whole map, the same since F-158 (ec043c92): layer 1
 * the board's DRAM, layer 2 the uMCTL2's state at the DRAM's start, the DDRC
 * and its PHY, the ROM (its API, executable), AIPS1..3 (and the 8MP's AIPS5),
 * the OCRAM, the GPV, the USB and AIPS4, layer 4 pb. Each region once:
 * xlat_tables sorts them, so the order they are added in doesn't change the
 * tables. The enable is ARMv8-A's */
static void test_mmu_map_regions(void **state)
{
    const unsigned int rw_nox = MT_RW | MT_MEMORY | MT_EXECUTE_NEVER;
    struct fake_mmap_region expected[24] = {
        { BOARD_RAM_BASE, BOARD_RAM_END - BOARD_RAM_BASE, rw_nox },
        { DDR_CSD1_BASE_ADDR, 128 * 1024, MT_MEMORY | MT_RW },
        { IMX8M_DDRPHY_BASE, 22 * 1024 * 1024, MT_DEVICE | MT_RW },
        { IMX8M_ROM_BASE, IMX8M_ROM_SIZE, MT_MEMORY | MT_RO | MT_EXECUTE },
        { IMX8M_AIPS_BASE, IMX8M_AIPS_SIZE, MT_DEVICE | MT_RW },
        { IMX8M_OCRAM_BASE, IMX8M_OCRAM_SIZE, MT_MEMORY | MT_RW },
        { IMX8M_GPV_BASE, IMX8M_GPV_SIZE, MT_DEVICE | MT_RW },
        { IMX8M_USB1_BASE, IMX8M_USB_SIZE, MT_DEVICE | MT_RW },
        { IMX8M_AIPS4_BASE, IMX8M_AIPS4_SIZE, MT_DEVICE | MT_RW },
    };
    size_t n = 9;
    (void)state;

    n += fake_mmap_pb_regions(&expected[n]);

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    fake_mmap_check_equal(expected, n);
    assert_int_equal(fake_mmu_enables, 1);
    assert_string_equal(fake_mmu_enabled_by, "el3");
}

/* platform.md 2.2, 5.2, board.md 2.8: a board region over one of
 * imx_mmap[]'s (AIPS4) fails pb_mmu_init() before anything is mapped, and
 * with it plat_init(): main() resets */
static void test_mmu_board_region_refused(void **state)
{
    (void)state;

    fake_board_mmap[0] = (struct pb_mmap_region)PB_MAP_DEVICE(IMX8M_AIPS4_BASE + 0x1000, 0x1000);
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), -PB_ERR_PARAM);
    assert_int_equal(fake_mmu_enables, 0);
    fake_board_mmap[0] = (struct pb_mmap_region)PB_MAP_END;
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_tick_from_early_init, setup),
        cmocka_unit_test_setup(test_tick_not_restarted_by_plat_init, setup),
        cmocka_unit_test_setup(test_status, setup),
        cmocka_unit_test_setup(test_status_tzasc_not_enabled, setup),
        cmocka_unit_test_setup(test_status_tzasc_region_not_open, setup),
        cmocka_unit_test_setup(test_status_rom_logs_missing, setup),
        cmocka_unit_test_setup(test_boot_source_serial_download, setup),
        cmocka_unit_test_setup(test_boot_source_not_a_download, setup),
        cmocka_unit_test_setup(test_boot_reason_kinds, setup),
        cmocka_unit_test_setup(test_boot_reason_priority, setup),
        cmocka_unit_test_setup(test_boot_reason_is_read_once_and_not_cleared, setup),
        cmocka_unit_test_setup(test_unique_id, setup),
        cmocka_unit_test_setup(test_unique_id_read_fails, setup),
        cmocka_unit_test_setup(test_plat_rot_and_slc, setup),
        cmocka_unit_test_setup(test_lock_waits_for_hab, setup),
        cmocka_unit_test_setup(test_ocotp_ipg_rate, setup),
        cmocka_unit_test_setup(test_crypto_set_up_last_in_plat_init, setup),
        cmocka_unit_test_setup(test_plat_init_succeeds_without_the_crypto_engine, setup),
        cmocka_unit_test_setup(test_status_crypto_engine, setup),
        cmocka_unit_test_setup(test_lockdown_stops_the_crypto_engine, setup),
        cmocka_unit_test_setup(test_mmu_map, setup),
        cmocka_unit_test_setup(test_mmu_map_regions, setup),
        cmocka_unit_test_setup(test_mmu_board_region_refused, setup),
        cmocka_unit_test_setup(test_get_info, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
