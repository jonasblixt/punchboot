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
 * fuses, the ROM's logs, the HAB, the TZASC and the MMU are fakes.
 *
 * Built twice: with CONFIG_ENABLE_WATCHDOG, as every board has it, and
 * without.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/fuse/imx_hab_fuses.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/rom_log/imx_hab_log.h>
#include <drivers/rom_log/imx_rom_log.h>
#include <drivers/tzc/tzc380.h>
#include <drivers/wdog/imx_wdog.h>
#include <pb/console.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imx6ul/imx6ul.h>
#include <string.h>

#include "fakes/regs.h"
#include "fakes/xlat.h"

/* The GPT (RM 28.6) */
#define GPT_CR     0x00
#define GPT_CR_SWR BIT(15)

/* A write that resets the SoC leaves the code under test through here */
static jmp_buf soc_reset;

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

int imx_ocotp_read(uint32_t bank, uint32_t row, uint32_t *value)
{
    (void)bank;

    if (row == IMX6UL_FUSE_SPEED_GRADE_WORD)
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

int tzc380_init(uintptr_t base)
{
    (void)base;
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
    memset(&f, 0, sizeof(f));
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

/* platform.md 3.11: what plat_get_info() reads, with jiffy's registers
 * (2026-11-14 and -16 on the board): a 6UL 1.2, PLL1 at 1056 MHz / 2, the
 * MMDC on PLL2's PFD2 at 396 MHz, SBMR1 0xc060: eMMC, 8 bits DDR */
static void test_get_info(void **state)
{
    struct pb_plat_info info;
    char extra[256];
    (void)state;

    assert_int_equal(plat_init(), PB_OK);
    fake_regs_set(IMX6UL_USB_ANALOG_DIGPROG, 0x00640002);
    fake_regs_set(IMX6UL_CCM_ANALOG_PLL_ARM, 88);
    fake_regs_set(IMX6UL_CCM_CACRR, 1);
    fake_regs_set(IMX6UL_CCM_CCSR, 0);
    fake_regs_set(IMX6UL_CCM_ANALOG_PLL_SYS, CCM_ANALOG_PLL_SYS_DIV_SELECT);
    fake_regs_set(IMX6UL_CCM_ANALOG_PFD_528, (24u << 16) | 27u);
    fake_regs_set(IMX6UL_CCM_CBCMR, 1u << 21);
    fake_regs_set(IMX6UL_CCM_CBCDR, 0);
    fake_regs_set(IMX6UL_SRC_BASE + 0x04, 0x0000c060);

    assert_int_equal(plat_get_info(&info, extra, sizeof(extra)), PB_OK);
    assert_string_equal(info.soc, "i.MX6UL");
    assert_string_equal(info.rev, "1.2");
    assert_int_equal(info.cpu_hz, 528000000);
    assert_int_equal(info.mem_hz, 396000000);
    assert_non_null(strstr(extra, "Boot config: SBMR1 0x0000c060"));
    assert_non_null(strstr(extra, "ROM eMMC boot: MMC, 8-bit DDR, normal speed, normal boot\n"));

    /* The 6ULL's DIGPROG, an SD card on four lines at high speed with
     * fast boot, the MMDC from PLL2 itself / 2 */
    fake_regs_set(IMX6UL_USB_ANALOG_DIGPROG, 0x00650101);
    fake_regs_set(IMX6UL_SRC_BASE + 0x04,
                  IMX6UL_BOOT_CFG_USDHC | IMX6UL_BOOT_CFG_FAST_BOOT |
                      IMX6UL_BOOT_CFG_MMC_HIGH_SPEED |
                      IMX6UL_BOOT_CFG_BUS_WIDTH(IMX6UL_BOOT_BUS_4BIT));
    fake_regs_set(IMX6UL_CCM_CBCMR, 0);
    fake_regs_set(IMX6UL_CCM_CBCDR, 1u << 3);
    assert_int_equal(plat_get_info(&info, extra, sizeof(extra)), PB_OK);
    assert_string_equal(info.soc, "i.MX6ULL");
    assert_string_equal(info.rev, "2.1");
    assert_int_equal(info.mem_hz, 264000000);
    assert_non_null(strstr(extra, "ROM eMMC boot: SD, 4-bit, high speed, fast boot\n"));

    /* periph2_clk2 from the oscillator; PFD2 gated; PLL4 isn't read */
    fake_regs_set(IMX6UL_CCM_CBCDR, CCM_CBCDR_PERIPH2_CLK_SEL);
    fake_regs_set(IMX6UL_CCM_CBCMR, CCM_CBCMR_PERIPH2_CLK2_SEL);
    assert_int_equal(plat_get_info(&info, extra, sizeof(extra)), PB_OK);
    assert_int_equal(info.mem_hz, 24000000);
    fake_regs_set(IMX6UL_CCM_CBCDR, 0);
    fake_regs_set(IMX6UL_CCM_CBCMR, 1u << 21);
    fake_regs_set(IMX6UL_CCM_ANALOG_PFD_528, (24u << 16) | BIT(23) | 27u);
    assert_int_equal(plat_get_info(&info, extra, sizeof(extra)), PB_OK);
    assert_int_equal(info.mem_hz, 0);
    fake_regs_set(IMX6UL_CCM_CBCMR, 3u << 21);
    assert_int_equal(plat_get_info(&info, extra, sizeof(extra)), PB_OK);
    assert_int_equal(info.mem_hz, 0);

    /* The core on the step clock: its rate isn't PLL1's */
    fake_regs_set(IMX6UL_CCM_CCSR, CCM_CCSR_PLL1_SW_CLK_SEL);
    assert_int_equal(plat_get_info(&info, extra, sizeof(extra)), PB_OK);
    assert_int_equal(info.cpu_hz, 0);

    /* Another SoC; and only whole lines go into the specifics */
    fake_regs_set(IMX6UL_USB_ANALOG_DIGPROG, 0x00630000);
    assert_int_equal(plat_get_info(&info, extra, 20), PB_OK);
    assert_string_equal(info.soc, "unknown");
    assert_string_equal(extra, "Speed grade 2\n");
    assert_int_equal(plat_get_info(&info, extra, 0), -PB_ERR_BUF_TOO_SMALL);
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

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_plat_reset, setup),
        cmocka_unit_test_setup(test_get_info, setup),
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
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
