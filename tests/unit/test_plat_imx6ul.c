/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX6UL port (src/plat/imx6ul/plat.c) against the platform contract
 * (doc/interfaces/platform.md), with the SoC's registers in a register file
 * (fakes/regs.h). The GPT's and the WDOG's drivers are the real ones; the
 * fuses, the ROM's logs, the HAB, the TZASC, the MMU and the crypto backends
 * (the CAAM and Mbed TLS: the test's build has both options) are fakes.
 *
 * Built twice: with CONFIG_ENABLE_WATCHDOG, as every board has it, and
 * without.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

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
#include <plat/imx6ul/imx6ul.h>
#include <string.h>

#include "fakes/regs.h"
#include "fakes/xlat.h"

/* The GPT (RM 28.6) */
#define GPT_CR     0x00
#define GPT_CR_SWR BIT(15)

/* A write that resets the SoC leaves the code under test through here */
static jmp_buf soc_reset;

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

/* The ROM's logs and the TZASC's driver: what each answers */
static struct {
    /* The events in the ROM's log, 0 ends them */
    uint32_t rom_events[4];
    bool no_rom_log;
    int hab_log_rc;
    int tzc_init_rc;
    int tzc_region_rc;
    unsigned int tzc_regions;
} f;

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

bool imx_rom_log_has_event(uint32_t event)
{
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

int hab_has_no_errors(void)
{
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

    if (bank == IMX6UL_FUSE_UNIQUE_BANK && row == IMX6UL_FUSE_UNIQUE1_WORD)
        *value = fuses.unique[0];
    else if (bank == IMX6UL_FUSE_UNIQUE_BANK && row == IMX6UL_FUSE_UNIQUE2_WORD)
        *value = fuses.unique[1];
    else if (bank == 0 && row == IMX6UL_FUSE_SPEED_GRADE_WORD)
        *value = 2 << IMX6UL_FUSE_SPEED_GRADE_SHIFT;
    else
        *value = 0;

    return PB_OK;
}

int imx_ocotp_lock(uint32_t sw_sticky, bool lock_scs)
{
    (void)sw_sticky;
    (void)lock_scs;
    return PB_OK;
}

void imx_hab_fuses_init(const struct imx_hab_fuses *cfg)
{
    (void)cfg;
}

/* The HAB's fuses and the OCOTP's access by index: the port only names them, in plat_rot and
 * plat_slc */
int imx_hab_read_key_status(const struct rot_key *key)
{
    (void)key;
    return PB_OK;
}

slc_t imx_hab_slc_read_status(void)
{
    return SLC_NOT_CONFIGURED;
}

int imx_hab_revoke_key(const struct rot_key *key)
{
    (void)key;
    return PB_OK;
}

int imx_hab_slc_set_configuration(void)
{
    return PB_OK;
}

int imx_hab_slc_set_configuration_locked(void)
{
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
    f.tzc_regions++;
    return f.tzc_region_rc;
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
    if (addr == IMX6UL_CCM_ANALOG_PLL_ARM)
        return value | CCM_ANALOG_PLL_ARM_LOCK;

    /* The GPT's software reset ends by itself */
    if (addr == IMX6UL_GPT1_BASE + GPT_CR)
        return value & ~GPT_CR_SWR;

    /* WDOG1's software reset: WCR.SRS written 0 */
    if (addr == IMX6UL_WDOG1_BASE + WDOG_WCR && (value & WDOG_WCR_SRS) == 0)
        longjmp(soc_reset, 1);

    return value;
}

static int setup(void **state)
{
    (void)state;
    memset(&fuses, 0, sizeof(fuses));
    memset(&f, 0, sizeof(f));
    memset(&crypto, 0, sizeof(crypto));
    memset(calls, 0, sizeof(calls));
    fake_regs_reset();
    fake_regs_on_write = soc_on_write;
    /* The board's DCD took the TZASC out of bypass */
    fake_regs_set(IMX6UL_IOMUXC_GPR9, IOMUXC_GPR9_TZASC1_BYP);
    fake_mmu_enables = 0;
    return 0;
}

/* What plat_get_boot_reason() makes of a value of SRC_SRSR */
static struct pb_boot_reason reason_of(uint32_t srsr)
{
    struct pb_boot_reason reason;

    memset(&reason, 0xff, sizeof(reason));
    fake_regs_set(IMX6UL_SRC_SRSR, srsr);
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_get_boot_reason(&reason), PB_OK);
    assert_int_equal(reason.raw, srsr);
    return reason;
}

/* platform.md 3.4: every cause of SRC_SRSR has a kind and a name, and raw is
 * the register (plat/imx6ul/imx6ul.h has the table) */
static void test_boot_reason_kinds(void **state)
{
    static const struct {
        uint32_t srsr;
        enum pb_boot_reason_kind kind;
        const char *name;
    } causes[] = {
        { IMX6UL_SRSR_POR, PB_BOOT_REASON_POR, "POR" },
        { IMX6UL_SRSR_CSU, PB_BOOT_REASON_FAULT, "CSU" },
        { IMX6UL_SRSR_USER, PB_BOOT_REASON_UNKNOWN, "User" },
        { IMX6UL_SRSR_WDOG, PB_BOOT_REASON_WDOG, "wdog" },
        { IMX6UL_SRSR_JTAG, PB_BOOT_REASON_DEBUG, "jtag_rst" },
        { IMX6UL_SRSR_JTAG_SW, PB_BOOT_REASON_DEBUG, "jtag_sw" },
        { IMX6UL_SRSR_WDOG3, PB_BOOT_REASON_WDOG, "wdog3" },
        { IMX6UL_SRSR_TEMPSENSE, PB_BOOT_REASON_FAULT, "temp" },
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

    reason = reason_of(IMX6UL_SRSR_POR | IMX6UL_SRSR_WDOG);
    assert_int_equal(reason.kind, PB_BOOT_REASON_WDOG);
    assert_string_equal(reason.name, "wdog");

    reason = reason_of(IMX6UL_SRSR_POR | IMX6UL_SRSR_USER);
    assert_int_equal(reason.kind, PB_BOOT_REASON_UNKNOWN);
    assert_string_equal(reason.name, "User");

    reason = reason_of(IMX6UL_SRSR_POR | IMX6UL_SRSR_WDOG | IMX6UL_SRSR_JTAG);
    assert_int_equal(reason.kind, PB_BOOT_REASON_DEBUG);
    assert_string_equal(reason.name, "jtag_rst");

    reason = reason_of(0x1fd);
    assert_int_equal(reason.kind, PB_BOOT_REASON_FAULT);
    assert_string_equal(reason.name, "temp");

    /* warm_boot is software's flag, not a cause: it is in raw only */
    reason = reason_of(IMX6UL_SRSR_WARM_BOOT | IMX6UL_SRSR_POR);
    assert_int_equal(reason.kind, PB_BOOT_REASON_POR);
    assert_string_equal(reason.name, "POR");

    reason = reason_of(IMX6UL_SRSR_WARM_BOOT);
    assert_int_equal(reason.kind, PB_BOOT_REASON_UNKNOWN);
}

/* platform.md 2.1, 3.4: the reason is read in plat_early_init() and never
 * cleared: the OS reads the same register (D-053) */
static void test_boot_reason_is_read_once_and_not_cleared(void **state)
{
    struct pb_boot_reason reason;
    (void)state;

    reason = reason_of(IMX6UL_SRSR_POR | IMX6UL_SRSR_WDOG);
    assert_int_equal(plat_init(), PB_OK);

    /* What the register reads later doesn't matter */
    fake_regs_set(IMX6UL_SRC_SRSR, IMX6UL_SRSR_POR);
    assert_int_equal(plat_get_boot_reason(&reason), PB_OK);
    assert_int_equal(reason.kind, PB_BOOT_REASON_WDOG);
    assert_int_equal(reason.raw, IMX6UL_SRSR_POR | IMX6UL_SRSR_WDOG);

    assert_int_equal(fake_regs_writes(IMX6UL_SRC_SRSR), 0);
}

/* platform.md 2.1: the watchdog is armed in plat_early_init() when
 * CONFIG_ENABLE_WATCHDOG is set, and only then. The WDOGs' power-down
 * counters are stopped whatever the option is */
static void test_wdog_armed_by_the_option(void **state)
{
    static const uintptr_t wdogs[] = { IMX6UL_WDOG1_BASE, IMX6UL_WDOG2_BASE, IMX6UL_WDOG3_BASE };
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);

#if CONFIG_ENABLE_WATCHDOG
    assert_int_equal(fake_regs_writes(IMX6UL_WDOG1_BASE + WDOG_WCR), 1);
    assert_true(fake_regs_get(IMX6UL_WDOG1_BASE + WDOG_WCR) & WDOG_WCR_WDE);
    assert_true(fake_regs_get(IMX6UL_WDOG1_BASE + WDOG_WCR) >> 8 != 0);
#else
    assert_int_equal(fake_regs_writes(IMX6UL_WDOG1_BASE + WDOG_WCR), 0);
#endif

    for (size_t i = 0; i < ARRAY_SIZE(wdogs); i++) {
        assert_true(fake_regs_writes(wdogs[i] + WDOG_WMCR) >= 1);
        assert_int_equal(fake_regs_get(wdogs[i] + WDOG_WMCR), 0);
    }

    /* Nothing went to the driver's base of old, 0 (F-206, F-234) */
    assert_int_equal(fake_regs_writes_in(0, 0x1000), 0);
}

/* What plat_get_status() answers, every field set by the port */
static struct plat_status status_now(void)
{
    struct plat_status status;

    memset(&status, 0xff, sizeof(status));
    plat_get_status(&status);
    return status;
}

/* platform.md 3.8: what plat_init() recorded. The TZASC is out of bypass and
 * DRAM is open in region 1, the watchdog is armed when the option is on and
 * reported as not armed without it, the ROM's logs were found, and the
 * platform sets no crypto engine up (the board does, F-247) */
static void test_status(void **state)
{
    struct plat_status status;
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    status = status_now();

    assert_int_equal(status.tz_dram_filter, PLAT_TZ_DRAM_ACTIVE);
    assert_int_equal(f.tzc_regions, 1);
#if CONFIG_ENABLE_WATCHDOG
    assert_true(status.wdog_armed);
#else
    assert_false(status.wdog_armed);
#endif
    assert_false(status.crypto_engine);
    assert_true(status.rom_log);
}

/* platform.md 2.2, 4.3 (F-241, F-248): a TZASC that the board's DCD left in
 * bypass isn't plat_init()'s error, command mode runs without it. It is
 * recorded, and no region is programmed */
static void test_status_tzasc_bypassed(void **state)
{
    (void)state;

    fake_regs_set(IMX6UL_IOMUXC_GPR9, 0);
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);

    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_BYPASSED);
    assert_int_equal(f.tzc_regions, 0);
}

/* The same for a set up that failed with the TZASC out of bypass: the
 * controller has no region, or refuses DRAM's. Neither is the state 4.3
 * asks for, and the boot is refused the same way */
static void test_status_tzasc_setup_failed(void **state)
{
    (void)state;

    f.tzc_init_rc = -PB_ERR_NOT_SUPPORTED;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_BYPASSED);

    f.tzc_init_rc = PB_OK;
    f.tzc_region_rc = -PB_ERR_PARAM;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_BYPASSED);

    /* And it is the state of this plat_init(), not of an earlier one */
    f.tzc_region_rc = PB_OK;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);
    assert_int_equal(status_now().tz_dram_filter, PLAT_TZ_DRAM_ACTIVE);
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

    f.hab_log_rc = PB_OK;
    assert_int_equal(plat_early_init(), PB_OK);
    assert_true(status_now().rom_log);
}

/* USB OTG1's device address register, and the address in it */
#define USB1_DEVICEADDR  (IMX6UL_USB1_BASE + 0x154)
#define DEVICEADDR_ADDR7 (7u << 25)

/* What plat_get_boot_source() answers after a plat_early_init() */
static enum pb_boot_source_kind boot_source_now(void)
{
    struct pb_boot_source src;

    memset(&src, 0xff, sizeof(src));
    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_get_boot_source(&src), PB_OK);
    return src.kind;
}

/* The ROM's log of a serial download, as jiffy's (test_imx_rom_log.c) */
static void rom_log_of_a_serial_download(void)
{
    f.rom_events[0] = IMX_ROM_LOG_V0_SERIAL_DOWNLOAD;
    f.rom_events[1] = IMX_ROM_LOG_V0_JUMP_SDP;
}

/* platform.md 3.9 (F-251): the ROM's serial downloader loaded pb, and a USB
 * host gave the ROM's device an address, as uuu does: USB */
static void test_boot_source_usb_download(void **state)
{
    (void)state;

    rom_log_of_a_serial_download();
    fake_regs_set(IMX6UL_CCM_CCGR6, CCM_CCGR6_USBOH3);
    fake_regs_set(USB1_DEVICEADDR, DEVICEADDR_ADDR7);
    assert_int_equal(boot_source_now(), PB_BOOT_SOURCE_USB);
}

/* The same without an address: the image came over the UART, the serial
 * downloader's other port. It was reported as unknown, which the core's
 * serial download stop doesn't stop for (board.md 5.1) */
static void test_boot_source_uart_download(void **state)
{
    (void)state;

    rom_log_of_a_serial_download();
    fake_regs_set(IMX6UL_CCM_CCGR6, CCM_CCGR6_USBOH3);
    fake_regs_set(USB1_DEVICEADDR, 0);
    assert_int_equal(boot_source_now(), PB_BOOT_SOURCE_UART);

    /* With the USB's clock gated its registers aren't read: no USB host */
    fake_regs_reset();
    fake_regs_on_write = soc_on_write;
    fake_regs_set(USB1_DEVICEADDR, DEVICEADDR_ADDR7);
    assert_int_equal(boot_source_now(), PB_BOOT_SOURCE_UART);
    assert_int_equal(fake_regs_reads(USB1_DEVICEADDR), 0);
}

/* Not a serial download: nothing of the USB is read, and without the eMMC's
 * driver in the build the partition isn't named. Unknown, which boots */
static void test_boot_source_not_a_download(void **state)
{
    (void)state;

    f.rom_events[0] = IMX_ROM_LOG_V0_PRIMARY_IMAGE;
    f.rom_events[1] = IMX_ROM_LOG_V0_PRIMARY_SD_MMC;
    f.rom_events[2] = IMX_ROM_LOG_V0_JUMP;
    fake_regs_set(IMX6UL_CCM_CCGR6, CCM_CCGR6_USBOH3);
    fake_regs_set(USB1_DEVICEADDR, DEVICEADDR_ADDR7);
    assert_int_equal(boot_source_now(), PB_BOOT_SOURCE_UNKNOWN);
    assert_int_equal(fake_regs_reads(USB1_DEVICEADDR), 0);

    /* No log at all */
    memset(f.rom_events, 0, sizeof(f.rom_events));
    assert_int_equal(boot_source_now(), PB_BOOT_SOURCE_UNKNOWN);
}

/* platform.md 3.3: plat_wdog_kick() is safe at any time, also before the
 * watchdog is armed and without CONFIG_ENABLE_WATCHDOG: WDOG1's service
 * sequence, at WDOG1 (F-206: it went to address 0 without the option) */
static void test_wdog_kick(void **state)
{
    (void)state;

    plat_wdog_kick();

    assert_int_equal(fake_reg_log_len, 2);
    assert_int_equal(fake_reg_log[0].addr, IMX6UL_WDOG1_BASE + WDOG_WSR);
    assert_int_equal(fake_reg_log[0].value, WDOG_WSR_SEQ1);
    assert_int_equal(fake_reg_log[1].addr, IMX6UL_WDOG1_BASE + WDOG_WSR);
    assert_int_equal(fake_reg_log[1].value, WDOG_WSR_SEQ2);
}

/* platform.md 3.1: plat_reset() resets the SoC whatever
 * CONFIG_ENABLE_WATCHDOG is, also before plat_early_init(): one write, to
 * WDOG1 (F-206: without the option it wrote to address 0 and spun) */
static void test_plat_reset(void **state)
{
    (void)state;

    if (setjmp(soc_reset) == 0) {
        plat_reset();
        fail_msg("plat_reset() returned");
    }

    assert_int_equal(fake_reg_log_len, 1);
    assert_int_equal(fake_reg_log[0].addr, IMX6UL_WDOG1_BASE + WDOG_WCR);
    assert_int_equal(fake_reg_log[0].value, WDOG_WCR_SRE | WDOG_WCR_WDE);
}

/* And after plat_init() */
static void test_plat_reset_after_init(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_int_equal(plat_init(), PB_OK);

    if (setjmp(soc_reset) == 0) {
        plat_reset();
        fail_msg("plat_reset() returned");
    }

    assert_int_equal(fake_reg_log[fake_reg_log_len - 1].addr, IMX6UL_WDOG1_BASE + WDOG_WCR);
    assert_int_equal(fake_reg_log[fake_reg_log_len - 1].value, WDOG_WCR_SRE | WDOG_WCR_WDE);
    assert_int_equal(fake_regs_writes_in(0, 0x1000), 0);
}

/* platform.md 3.5, 3.7: the unique id is the two UNIQUE_ID words (bank 0,
 * words 1 and 2), the first word first, each with its low byte first: eight
 * bytes that never change, because the device's UUID is made from them */
static void test_unique_id(void **state)
{
    static const uint8_t expected[8] = { 0x04, 0x03, 0x02, 0x01, 0x08, 0x07, 0x06, 0x05 };
    uint8_t id[16];
    size_t length = sizeof(id);
    (void)state;

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
 * Until F-247 jiffy's init() did the CAAM's two calls as its first step,
 * which was the first thing after plat_init(): they are plat_init()'s last
 * now, after the TZASC and with the MMU on, and no register of the
 * platform's is written after them. So the SoC sees the same writes in the
 * same order */
static void test_crypto_set_up_last_in_plat_init(void **state)
{
    (void)state;

    assert_int_equal(plat_early_init(), PB_OK);
    assert_string_equal(calls, "");

    assert_int_equal(plat_init(), PB_OK);
    assert_string_equal(calls, "tzasc caam rng mbedtls ");
    assert_int_equal(crypto.caam_base, IMX6UL_CAAM_JR0_BASE);
    assert_int_equal(crypto.rng_base, IMX6UL_CAAM_BASE);
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

/* platform.md 4.1, 4.2 (F-247): the root of trust and the life cycle are the
 * platform's, HABv4's state in the OCOTP. What jiffy wired itself until
 * then, unchanged: SRK revocation, the SRK hash and the closed bit, the
 * fuses by index, and no EOL (F-263) */
static void test_plat_rot_and_slc(void **state)
{
    (void)state;

    assert_ptr_equal(plat_rot.revoke_key, imx_hab_revoke_key);
    assert_ptr_equal(plat_rot.read_key_status, imx_hab_read_key_status);

    assert_ptr_equal(plat_slc.read_status, imx_hab_slc_read_status);
    assert_ptr_equal(plat_slc.set_configuration, imx_hab_slc_set_configuration);
    assert_ptr_equal(plat_slc.set_configuration_locked, imx_hab_slc_set_configuration_locked);
    assert_null(plat_slc.set_eol);
    assert_ptr_equal(plat_slc.fuse_read, imx_ocotp_fuse_read);
    assert_ptr_equal(plat_slc.fuse_write, imx_ocotp_fuse_write);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_plat_reset, setup),
        cmocka_unit_test_setup(test_wdog_kick, setup),
        cmocka_unit_test_setup(test_wdog_armed_by_the_option, setup),
        cmocka_unit_test_setup(test_status, setup),
        cmocka_unit_test_setup(test_status_tzasc_bypassed, setup),
        cmocka_unit_test_setup(test_status_tzasc_setup_failed, setup),
        cmocka_unit_test_setup(test_status_rom_logs_missing, setup),
        cmocka_unit_test_setup(test_boot_source_usb_download, setup),
        cmocka_unit_test_setup(test_boot_source_uart_download, setup),
        cmocka_unit_test_setup(test_boot_source_not_a_download, setup),
        cmocka_unit_test_setup(test_plat_reset_after_init, setup),
        cmocka_unit_test_setup(test_boot_reason_kinds, setup),
        cmocka_unit_test_setup(test_boot_reason_priority, setup),
        cmocka_unit_test_setup(test_boot_reason_is_read_once_and_not_cleared, setup),
        cmocka_unit_test_setup(test_unique_id, setup),
        cmocka_unit_test_setup(test_unique_id_read_fails, setup),
        cmocka_unit_test_setup(test_unique_id_buffer_too_small, setup),
        cmocka_unit_test_setup(test_plat_rot_and_slc, setup),
        cmocka_unit_test_setup(test_crypto_set_up_last_in_plat_init, setup),
        cmocka_unit_test_setup(test_plat_init_succeeds_without_the_crypto_engine, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
