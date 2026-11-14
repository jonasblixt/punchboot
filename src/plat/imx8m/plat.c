/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <arch/arch_helpers.h>
#include <boot/atf.h>
#include <drivers/crypto/imx_caam.h>
#include <drivers/crypto/mbedtls.h>
#include <drivers/fuse/imx_hab_fuses.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/rom_log/imx_hab_log.h>
#include <drivers/rom_log/imx_rom_log.h>
#include <drivers/timer/imx_gpt.h>
#include <drivers/tzc/tzc380.h>
#include <drivers/usb/dwc3_udc.h>
#include <drivers/usb/imx8m_phy.h>
#include <drivers/wdog/imx_wdog.h>
#include <inttypes.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/mmu.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <plat/imx8m/imx8m.h>
#include <stdio.h>
#include <uuid.h>
#include <xlat_tables.h>

#include <plat/imx8m/dram.h>

#include "plat.h"

/* System counter control frame */
#define SCTR_CNTCR      0x00
#define SCTR_CNTCR_EN   BIT(0) /* Counting */
#define SCTR_CNTCR_HDBG BIT(1) /* Halt while the core is halted in debug */
#define SCTR_CNTCR_FCR0 BIT(8) /* Count at the base frequency, CNTFID0 */
#define SCTR_CNTCR_FCR1 BIT(9) /* ... at CNTFID1 */
#define SCTR_CNTFID0    0x20

#ifdef CONFIG_BOOT_ATF
/*
 * TF-A's bl_params (boot/atf.h, platform.md 5.3). The i.MX8M's BL31 takes
 * them only from the low part of the OCRAM, and otherwise starts BL33 at
 * its own fixed address without them: on the i.MX8M Plus below 0x960000
 * (v2.12 imx8mp_bl31_setup.c swaps base and size in that check). They are
 * in .data, which follows the code, not at the end of .bss, and link.lds
 * checks where that is. The i.MX8MQ's BL31 takes none upstream (v2.12 to
 * v2.15): with the test bench's patch from the TCM or the OCRAM below BL31,
 * and pb's .data is in the TCM.
 */
uint8_t plat_bl_params[PLAT_BL_PARAMS_SIZE] __section(".data") __aligned(PLAT_BL_PARAMS_ALIGN);
#endif

const char *platform_ns_uuid = "\x32\x92\xd7\xd2\x28\x25\x41\x00\x90\xc3\x96\x8f\x29\x60\xc9\xf2";


static const mmap_region_t imx_mmap[] = {
    /* UMCTL2 state, at the start of the DRAM */
    MAP_REGION_FLAT(DDR_CSD1_BASE_ADDR, (128 * 1024), MT_MEMORY | MT_RW),
    /* DDRC and its PHY */
    MAP_REGION_FLAT(IMX8M_DDRPHY_BASE, (22 * 1024 * 1024), MT_DEVICE | MT_RW),
    /* Boot ROM API*/
    MAP_REGION_FLAT(IMX8M_ROM_BASE, IMX8M_ROM_SIZE, MT_MEMORY | MT_RO | MT_EXECUTE),
    /* Periph (AIPS) */
    MAP_REGION_FLAT(IMX8M_AIPS_BASE, IMX8M_AIPS_SIZE, MT_DEVICE | MT_RW),
    /* OCRAM */
    MAP_REGION_FLAT(IMX8M_OCRAM_BASE, IMX8M_OCRAM_SIZE, MT_MEMORY | MT_RW),
    /* GVP */
    MAP_REGION_FLAT(IMX8M_GPV_BASE, IMX8M_GPV_SIZE, MT_DEVICE | MT_RW),
    /* USB */
    MAP_REGION_FLAT(IMX8M_USB1_BASE, IMX8M_USB_SIZE, MT_DEVICE | MT_RW),
    /* AIPS4, whole (platform.md 5.2, layer 2): the TZASC and the 8MP's HSIO
     * block control, which was its only part mapped. The span F-158 split
     * covered the rest as Normal memory (Linux's imx8mq.dtsi and
     * imx8mp.dtsi, bus@32c00000) */
    MAP_REGION_FLAT(IMX8M_AIPS4_BASE, IMX8M_AIPS4_SIZE, MT_DEVICE | MT_RW),
    { 0 }
};

/* SRC_SRSR as plat_early_init() read it */
static uint32_t srsr;

/* What plat_early_init() and plat_init() recorded (platform.md 3.8). The
 * TZASC counts as bypassed until plat_init() has shown that it isn't */
static struct plat_status status = {
    .tz_dram_filter = PLAT_TZ_DRAM_BYPASSED,
};

void plat_get_status(struct plat_status *out)
{
    *out = status;
}

/* The ROM's event log was saved: a dump into no room fails with
 * -PB_ERR_NOT_FOUND only when there is no log, which it logs */
static bool rom_log_found(void)
{
    char none;
    size_t length = 0;

    return imx_rom_log_dump(&none, &length) != -PB_ERR_NOT_FOUND;
}

/* The causes, the highest priority first (plat/imx8m/mm.h) */
static const struct {
    uint32_t bit;
    enum pb_boot_reason_kind kind;
    const char *name;
} boot_reasons[] = {
    { SRC_SRSR_TEMP_RST_B, PB_BOOT_REASON_FAULT, "temp" },
#ifdef CONFIG_PLAT_IMX8MQ
    { SRC_SRSR_WDOG4_RST_B, PB_BOOT_REASON_WDOG, "wdog4" },
#else
    { SRC_SRSR_WDOG4_RST_B, PB_BOOT_REASON_WDOG, "wdog2" },
#endif
    { SRC_SRSR_WDOG3_RST_B, PB_BOOT_REASON_WDOG, "wdog3" },
    { SRC_SRSR_JTAG_SW_RST, PB_BOOT_REASON_DEBUG, "jtag_sw" },
    { SRC_SRSR_JTAG_RST_B, PB_BOOT_REASON_DEBUG, "jtag_rst" },
    { SRC_SRSR_WDOG1_RST_B, PB_BOOT_REASON_WDOG, "wdog" },
    { SRC_SRSR_IPP_USER_RST_B, PB_BOOT_REASON_UNKNOWN, "User" },
    { SRC_SRSR_CSU_RST_B, PB_BOOT_REASON_FAULT, "CSU" },
    { SRC_SRSR_IPP_RST_B, PB_BOOT_REASON_POR, "POR" },
};

int plat_get_boot_reason(struct pb_boot_reason *reason)
{
    reason->kind = PB_BOOT_REASON_UNKNOWN;
    reason->raw = srsr;
    reason->name = "Unknown";

    for (size_t i = 0; i < ARRAY_SIZE(boot_reasons); i++) {
        if (srsr & boot_reasons[i].bit) {
            reason->kind = boot_reasons[i].kind;
            reason->name = boot_reasons[i].name;
            break;
        }
    }

    return PB_OK;
}

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    union {
        uint32_t uid[2];
        uint8_t uid_bytes[8];
    } plat_unique;

    int rc;

    if (*length < sizeof(plat_unique.uid_bytes))
        return -PB_ERR_BUF_TOO_SMALL;

    /* A read that fails writes nothing: the device's UUID is made of this */
    rc = imx_ocotp_read(IMX8M_FUSE_UNIQUE_BANK, IMX8M_FUSE_UNIQUE1_WORD, &plat_unique.uid[0]);

    if (rc != PB_OK)
        return rc;

    rc = imx_ocotp_read(IMX8M_FUSE_UNIQUE_BANK, IMX8M_FUSE_UNIQUE2_WORD, &plat_unique.uid[1]);

    if (rc != PB_OK)
        return rc;

    memcpy(output, plat_unique.uid_bytes, sizeof(plat_unique.uid_bytes));
    *length = sizeof(plat_unique.uid_bytes);
    return PB_OK;
}

void plat_reset(void)
{
    imx_wdog_reset_now(IMX8M_WDOG1_BASE);
}

uint32_t plat_get_us_tick(void)
{
    return imx_gpt_get_tick();
}

void plat_wdog_kick(void)
{
    imx_wdog_kick(IMX8M_WDOG1_BASE);
}

/* The ROM log, AN12853 Tables 11 and 13. It's in the ROM's part of the OCRAM,
 * below pb. The 8MQ's is 64 words: its boot information (Boot_SW_Info) is
 * 0x100 bytes after it (the pointers at ROM 0x960 and 0x968, mm.h) */
static const struct imx_rom_log_config rom_log_cfg = {
    .pointer = IMX8M_ROM_LOG_POINTER,
    .ram_start = IMX8M_OCRAM_BASE,
    .ram_end = IMX8M_OCRAM_BASE + IMX8M_OCRAM_SIZE,
#ifdef CONFIG_PLAT_IMX8MQ
    .words = 64,
    .defs = IMX_ROM_LOG_DEFS_V2_8MQ,
#else
    .words = 128,
    .defs = IMX_ROM_LOG_DEFS_V2,
#endif
};

/* The HAB API. 64-bit pointers; the address is U-Boot's (the RMs don't have
 * it), imx_hab_log_init() checks the RVT header before using it */
static const struct imx_hab_log_config hab_log_cfg = {
#ifdef CONFIG_PLAT_IMX8MQ
    .rvt = IMX8MQ_HAB_RVT_BASE,
#else
    .rvt = IMX8MP_HAB_RVT_BASE,
#endif
    .entry_size = 8,
};

#ifdef CONFIG_PLAT_IMX8MQ
#define DDR_WINDOW_SIZE IMX8MQ_DDR_WINDOW_SIZE
#else
#define DDR_WINDOW_SIZE IMX8MP_DDR_WINDOW_SIZE
#endif

/*
 * The TZASC checks no DRAM access until GPR10's TZASC_EN is set, and TF-A's
 * BL31 only programs it when it is. Set before DRAM is used, with
 * ID_SWAP_BYPASS, which the RM asks for (8MP RM 4.9.1.2), and lock both:
 * the normal world reaches the GPRs. OP-TEE checks all three.
 */
static int tzasc_enable(void)
{
    const uint32_t bits = GPR10_TZASC_EN | GPR10_TZASC_ID_SWAP_BYPASS;
    const uint32_t locks = GPR10_LOCK_TZASC_EN | GPR10_LOCK_TZASC_ID_SWAP;
    uint32_t gpr10;

    mmio_clrsetbits_32(IMX8M_IOMUXC_GPR10, 0, bits);
    mmio_clrsetbits_32(IMX8M_IOMUXC_GPR10, 0, locks);
    gpr10 = mmio_read_32(IMX8M_IOMUXC_GPR10);

    if ((gpr10 & (bits | locks)) != (bits | locks)) {
        LOG_ERR("TZASC not enabled, GPR10 %08" PRIx32, gpr10);
        return -PB_ERR_STATE;
    }

    return PB_OK;
}

/*
 * Out of bypass, region 0 lets only secure accesses through, and some
 * masters are non-secure: the DWC3, command mode's USB, among them. Open the
 * DDR window to both worlds in region 1 and leave region 0 secure, so that
 * aliases of DRAM stay protected; OP-TEE replaces region 1 with its own. The
 * TZASC counts from the DDR window's start.
 */
static int tzasc_open_dram(void)
{
    uint64_t size;
    int rc;

    rc = tzc380_init(IMX8M_TZASC_BASE);

    if (rc != PB_OK)
        return rc;

    /* The enable is locked: without region 1 command mode's USB stops. So
     * if the TZASC addresses less than the window, open what it addresses */
    for (size = DDR_WINDOW_SIZE; size >= SZ_GiB(1); size >>= 1) {
        rc = tzc380_configure_region(1, 0, size, TZC380_SP_ALL);

        if (rc != -PB_ERR_PARAM)
            break;
    }

    if (rc == PB_OK && size != DDR_WINDOW_SIZE)
        LOG_ERR("TZASC: only %" PRIu64 " MiB of DRAM opened", size >> 20);

    return rc;
}

/* HABv4's SRK_REVOKE[2:0] (platform.md 4.1) */
#define IMX8M_REVOCABLE_SRKS (3)

/* HABv4's state in the OCOTP (fusebox.h, plat-imx.md 4.1) */
static const struct imx_hab_fuses hab_fuses = {
    .sec_config_bank = IMX8M_FUSE_SEC_CONFIG_BANK,
    .sec_config_word = IMX8M_FUSE_SEC_CONFIG_WORD,
    .closed = IMX8M_FUSE_SEC_CONFIG_CLOSED,
    .srk_bank = IMX8M_FUSE_SRK_BANK,
    .srk_word = IMX8M_FUSE_SRK_WORD,
#ifdef CONFIG_PLAT_IMX8MQ
    .revoke_bank = IMX8MQ_FUSE_REVOKE_BANK,
    .revoke_word = IMX8MQ_FUSE_REVOKE_WORD,
    .revocable = IMX8M_REVOCABLE_SRKS,
#endif
    .revoke_allowed = imx_hab_no_failure,
};

/*
 * The root of trust and the life cycle (platform.md 4.1, 4.2): HABv4's fuses.
 * The 8MQ revokes keys through SRK_REVOKE. The 8M Plus's SRK_REVOKE isn't in
 * its public RM: its keys read as active and none is revoked (fusebox.h).
 * No EOL (F-263).
 */
#ifdef CONFIG_PLAT_IMX8MP
static int read_key_status(const struct rot_key *key)
{
    (void)key;
    return PB_OK;
}
#endif

/* The build's warning for a board key in a slot without a bit reads the
 * makefile.mk's count (platform.md 4.1) */
#ifdef PLAT_REVOKE_SLOTS
_Static_assert(PLAT_REVOKE_SLOTS == IMX8M_REVOCABLE_SRKS,
               "makefile.mk's PLAT_REVOKE_SLOTS is HAB's");
#endif

const struct rot_config plat_rot = {
#ifdef CONFIG_PLAT_IMX8MQ
    .revoke_key = imx_hab_revoke_key,
    .read_key_status = imx_hab_read_key_status,
#else
    .read_key_status = read_key_status,
#endif
    .revoke_slots = IMX8M_REVOCABLE_SRKS,
};

/* Closing a device whose image HAB doesn't accept would leave one that
 * doesn't boot: the lock waits for a boot without a HAB failure */
static int imx8m_slc_lock(void)
{
    int rc = imx_hab_no_failure();

    if (rc != PB_OK)
        return rc;

    return imx_hab_slc_set_configuration_locked();
}

const struct slc_ops plat_slc = {
    .read_status = imx_hab_slc_read_status,
    .set_configuration = imx_hab_slc_set_configuration,
    .set_configuration_locked = imx8m_slc_lock,
    .fuse_read = imx_ocotp_fuse_read,
    .fuse_write = imx_ocotp_fuse_write,
};

/*
 * The build's crypto backends (platform.md 2.2, crypto.md 2), each behind
 * its driver's option: the CAAM on job ring 0 with its RNG, and Mbed TLS. A
 * board that needs the software implementation turns the CAAM's option off
 * and Mbed TLS's on in its defconfig. The ROM leaves the CAAM's clock on.
 *
 * Command mode can start without a backend, so a failure isn't plat_init()'s
 * error: main() would reset. A backend that failed isn't registered and no
 * image verifies: it is recorded (plat_get_status()), and the boot's
 * preconditions refuse the boot before anything is loaded (platform.md 2.3).
 *
 * Returns whether the build's backends all came up. A build without one has
 * none that works.
 */
static bool crypto_init(void)
{
    unsigned int backends = 0;
    bool failed = false;
    int rc = PB_OK;

#ifdef CONFIG_DRIVERS_IMX_CAAM
    rc = imx_caam_init(IMX8M_CAAM_JR0_BASE);
    backends++;

    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        failed = true;
    } else {
        /* Linux can't instantiate the RNG when it runs in the normal world
         * (platform.md 4.5). Without it Linux has no hardware RNG, which is
         * no reason not to boot */
        rc = imx_caam_rng_init(IMX8M_CAAM_BASE);

        if (rc != PB_OK)
            LOG_ERR("CAAM RNG init failed (%i)", rc);
    }
#endif

#ifdef CONFIG_DRIVERS_CRYPTO_MBEDTLS
    rc = mbedtls_pb_init();
    backends++;

    if (rc != PB_OK) {
        LOG_ERR("Mbed TLS init failed (%i)", rc);
        failed = true;
    }
#endif

    (void)rc;
    return backends > 0 && !failed;
}

/*
 * Before the jump the platform stops what plat_init() started (platform.md
 * 4.5, drivers.md 2.3): the CAAM's job ring holds no address in pb's memory
 * any more (crypto.md 6). Mbed TLS has nothing to stop. After it the CAAM's
 * backend answers -PB_ERR_STATE until a reset. A stop that fails is a failed
 * boot: plat_lockdown() returns its error and pb doesn't jump.
 */
static int crypto_stop(void)
{
#ifdef CONFIG_DRIVERS_IMX_CAAM
    int rc = imx_caam_stop();

    if (rc != PB_OK)
        LOG_ERR("CAAM stop failed (%i)", rc);

    return rc;
#else
    return PB_OK;
#endif
}

int plat_early_init(void)
{
    /* The reset reason. Read only: the OS reads the same register. The SRC
     * has no clock gate */
    srsr = mmio_read_32(IMX8M_SRC_SRSR);

    /* Both of the ROM's logs, or it is recorded: logged, nothing of the
     * boot's rests on them (platform.md 2.3) */
    imx_rom_log_save(&rom_log_cfg);
    status.rom_log = rom_log_found();

    if (imx_hab_log_init(&hab_log_cfg) != PB_OK)
        status.rom_log = false;

    /* The WDOGs' power-down counters, whether the watchdog is used or not */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(WDOG_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_WDOG1), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_WDOG2), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_WDOG3), CCGR_CLK_ON_MASK);
    imx_wdog_disable_powerdown(IMX8M_WDOG1_BASE);
    imx_wdog_disable_powerdown(IMX8M_WDOG2_BASE);
    imx_wdog_disable_powerdown(IMX8M_WDOG3_BASE);

    status.wdog_armed = false;
#ifdef CONFIG_ENABLE_WATCHDOG
    /* Enable and ungate WDOG1 clock */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(WDOG_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_WDOG1), CCGR_CLK_ON_MASK);
    status.wdog_armed = (imx_wdog_init(IMX8M_WDOG1_BASE, CONFIG_WATCHDOG_TIMEOUT) == PB_OK);
#endif

    /* Configure systick. GPT1_CLK_ROOT is on out of reset, on the 24 MHz
     * reference (CCM_TARGET_ROOT107 resets to 0x10000000, RM 5.1.7.10) */
    mmio_write_32(CCM_CCGR_SET(CCGR_GPT1), CCGR_CLK_ON_MASK);
    imx_gpt_init(IMX8M_GPT1_BASE, MHz(24));
    log_time_start();
    return PB_OK;
}

/* A clock root's target register, root n of the CCM (8MQ RM 5.1) */
#define IPG_ROOT_POST_DIV_MASK 0x3u /* IPG_CLK_ROOT divides AHB_CLK_ROOT */
#define CCM_ROOT_TARGET(n)     (0x30388000UL + 0x80UL * (n))

/* The 24 or 25 MHz reference, AHB_CLK_ROOT's source 0 */
#ifdef CONFIG_PLAT_IMX8MQ
#define IMX8M_OSC_HZ 25000000u
#else
#define IMX8M_OSC_HZ 24000000u
#endif

/*
 * The OCOTP's ipg_clk, which its fuse timing is made for (imx_ocotp_init()):
 * IPG_CLK_ROOT, AHB_CLK_ROOT divided by its POST_PODF[1:0] + 1 (NXP's
 * U-Boot, clock_imx8mq.c), as the ROM left both; pb doesn't change them.
 * AHB_CLK_ROOT's sources 0 to 4 are fixed (8MQ RM, the clock root table:
 * the reference, SYSTEM_PLL1_DIV6, SYSTEM_PLL1, SYSTEM_PLL1_DIV2,
 * SYSTEM_PLL2_DIV8; the 8M Plus's the same). Another source: 0, and pb
 * writes no fuse. The pico8ml's ROM leaves source 1 and IPG at AHB / 2,
 * 66.7 MHz
 */
static unsigned int ipg_rate_hz(void)
{
    static const uint32_t ahb_src_hz[] = {
        IMX8M_OSC_HZ, 133333333u, 800000000u, 400000000u, 125000000u,
    };
    uint32_t ahb = mmio_read_32(CCM_ROOT_TARGET(AHB_CLK_ROOT));
    uint32_t ipg = mmio_read_32(CCM_ROOT_TARGET(IPG_CLK_ROOT));
    unsigned int src = (ahb & CLK_ROOT_SRC_MUX_MASK) >> CLK_ROOT_SRC_MUX_SHIFT;
    unsigned int pre = ((ahb & CLK_ROOT_PRE_DIV_MASK) >> 16) + 1;
    unsigned int post = ((ahb & CLK_ROOT_POST_DIV_MASK) >> CLK_ROOT_POST_DIV_SHIFT) + 1;

    if (src >= ARRAY_SIZE(ahb_src_hz)) {
        LOG_ERR("AHB_CLK_ROOT from source %u: no fuse timing, no fuse is written", src);
        return 0;
    }

    return ahb_src_hz[src] / pre / post / ((ipg & IPG_ROOT_POST_DIV_MASK) + 1);
}

int plat_init(void)
{
    int tzasc_rc;
    int rc;

    /* Init fusebox, its clock gate may be closed. Its fuse timing is for
     * the ipg_clk the ROM left: fuse writes (the life cycle, plat_slc) */
    mmio_write_32(CCM_CCGR_SET(CCGR_OCOTP), CCGR_CLK_ON_MASK);
    imx_ocotp_init(IMX8M_OCOTP_BASE, IMX8M_FUSE_WORDS_PER_BANK, ipg_rate_hz());
    imx_hab_fuses_init(&hab_fuses);

    /*
     * The system counter, which the Arm generic timer counts: TF-A's delays
     * and Linux's arch timer need it running, and neither the ROM nor pb
     * started it. Its base frequency is in CNTFID0 (8 MHz, the 24 MHz
     * oscillator / 3).
     */
    mmio_write_32(CCM_CCGR_SET(CCGR_SCTR), CCGR_CLK_ON_MASK);
    write_cntfrq_el0(mmio_read_32(IMX8M_SCTR_BASE + SCTR_CNTFID0));
    mmio_clrsetbits_32(IMX8M_SCTR_BASE + SCTR_CNTCR,
                       SCTR_CNTCR_FCR1,
                       SCTR_CNTCR_FCR0 | SCTR_CNTCR_HDBG | SCTR_CNTCR_EN);

    /* Ungate GPIO clocks */
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO1), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO2), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO3), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO4), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_GPIO5), CCGR_CLK_ON_MASK);

    /* Ungate IOMUX clocks */
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX1), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX2), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_IOMUX3), CCGR_CLK_ON_MASK);

    /* The rest of the gates and the PLLs */
    imx8m_soc_clock_init();

    tzasc_rc = tzasc_enable();

    rc = imx8m_ddr_init();
    if (rc != PB_OK) {
        LOG_ERR("LPDDR4 init failed (%i)", rc);
        return rc;
    }

    LOG_DBG("LPDDR4 training complete");

    /* Not plat_init()'s error: command mode's prerequisites are there, and
     * main() would reset (platform.md 2.2). It is recorded, with the enable
     * above, and no boot goes on without both (platform.md 2.3) */
    rc = tzasc_open_dram();

    if (rc != PB_OK)
        LOG_ERR("TZASC setup failed (%i)", rc);

    status.tz_dram_filter = (tzasc_rc == PB_OK && rc == PB_OK) ? PLAT_TZ_DRAM_ACTIVE
                                                               : PLAT_TZ_DRAM_BYPASSED;

    imx_wdog_kick(IMX8M_WDOG1_BASE);

    /* The memory map, imx_mmap[] as its layer 2 (platform.md 5.2). Without
     * it command mode doesn't run: plat_init()'s error (2.2) */
    rc = pb_mmu_init(imx_mmap);

    if (rc != PB_OK) {
        LOG_ERR("MMU setup failed (%i)", rc);
        return rc;
    }

    /* Last: until F-247 the board's init() did it, as its first step */
    status.crypto_engine = crypto_init();

    LOG_DBG("Plat init done");
    return PB_OK;
}

#ifdef CONFIG_IMX8M_CM_USB1
/* Command mode's USB (platform.md 3.10): USB1, a DWC3 with the SoC's PHY.
 * Its clocks and power differ per SoC */
int plat_cm_init(void)
{
    int rc;

    rc = imx8m_soc_usb1_power_up();

    if (rc != PB_OK)
        return rc;

    rc = imx8m_usb_phy_init(IMX8M_USB1_PHY_BASE);

    if (rc != PB_OK) {
        LOG_ERR("USB PHY init failed (%i)", rc);
        return rc;
    }

    return imx_dwc3_udc_init(IMX8M_USB1_BASE);
}
#endif

/* The ROM's event when it starts its serial downloader, in the i.MX 8M's
 * event definitions (AN12853 Table 9, "Serial download") */
#define ROM_LOG_V2_SERIAL_DOWNLOAD 0xd0

/*
 * Where pb was loaded from (platform.md 3.9), from the ROM's event log,
 * which plat_early_init() saved. The ROM logs when it starts its serial
 * downloader, and what it loads then comes over USB: the i.MX 8M's serial
 * downloader has no UART. Without that event, and without a log, it is a
 * boot from the boot device, whose partition isn't named: unknown.
 */
int plat_get_boot_source(struct pb_boot_source *src)
{
    memset(src, 0, sizeof(*src));
    src->kind = PB_BOOT_SOURCE_UNKNOWN;

    if (imx_rom_log_has_event(ROM_LOG_V2_SERIAL_DOWNLOAD))
        src->kind = PB_BOOT_SOURCE_USB;

    return PB_OK;
}

int plat_lockdown(void)
{
    int rc;

    /*
     * D-084, as the 6UL: the CSF unlocks SRK revocation for pb's command mode
     * (pb.csf.template), so it is locked again before the next stage runs,
     * with field return, in SW_STICKY. SCS holds HAB_JDE, which HAB "must
     * lock ... before passing control to the OS" (8MQ RM 6.3.5, OCOTP_SCS).
     * NXP's U-Boot does the same on a closed 8MQ (secure_lockup()). The 8M
     * family's OCOTP has SW_STICKY and SCS where the 8MQ has them (U-Boot's
     * struct ocotp_regs). SW_STICKY holds until a reset, SCS's lock until a
     * POR.
     */
    rc = imx_ocotp_lock(OCOTP_SW_STICKY_SRK_REVOKE_LOCK | OCOTP_SW_STICKY_FIELD_RETURN_LOCK, true);

    if (rc != PB_OK)
        return rc;

    /* Last, and only when the locks are set: a lockdown that fails ends in
     * command mode, where a locked device verifies its token with the CAAM */
    return crypto_stop();
}

/* platform.md 3.11. DIGPROG's MINOR is the base layer in 7:4 and the metal
 * layer in 3:0 (RM 5.1.8, DIGPROG): 0x10 is 1.0. The DRAM's clock is half the
 * data rate it was trained at, the board's first rate (struct dram_rates) */
int plat_get_info(struct pb_plat_info *info, char *extra, size_t extra_size)
{
    static char rev[8];
    uint32_t minor = imx8m_soc_digprog() & 0xff;

    if (extra_size == 0)
        return -PB_ERR_BUF_TOO_SMALL;
    extra[0] = '\0';

    info->soc = imx8m_soc_name();
    (void)snprintf(
        rev, sizeof(rev), "%u.%u", (unsigned int)(minor >> 4), (unsigned int)(minor & 0xf));
    info->rev = rev;
    info->cpu_hz = imx8m_soc_cpu_hz();
    info->mem_hz = (dram_rates.num > 0) ? dram_rates.mts[0] * 500000u : 0;

    return PB_OK;
}
