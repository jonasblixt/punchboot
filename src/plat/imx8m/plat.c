/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <arch/arch_helpers.h>
#include <boot/atf.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/rom_log/imx_hab_log.h>
#include <drivers/rom_log/imx_rom_log.h>
#include <drivers/timer/imx_gpt.h>
#include <drivers/tzc/tzc380.h>
#include <drivers/wdog/imx_wdog.h>
#include <inttypes.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imx8m/imx8m.h>
#include <uuid.h>
#include <xlat_tables.h>

#include <plat/imx8m/dram.h>

#include "plat.h"

IMPORT_SYM(uintptr_t, _code_start, code_start);
IMPORT_SYM(uintptr_t, _code_end, code_end);
IMPORT_SYM(uintptr_t, _data_region_start, data_start);
IMPORT_SYM(uintptr_t, _data_region_end, data_end);
IMPORT_SYM(uintptr_t, _ro_data_region_start, ro_data_start);
IMPORT_SYM(uintptr_t, _ro_data_region_end, ro_data_end);
IMPORT_SYM(uintptr_t, _stack_start, stack_start);
IMPORT_SYM(uintptr_t, _stack_end, stack_end);
IMPORT_SYM(uintptr_t, _zero_region_start, rw_nox_start);
IMPORT_SYM(uintptr_t, _no_init_end, rw_nox_end);

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
 * checks where that is.
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
    /* HSIO BLK_CTRL (i.MX8M Plus) */
    MAP_REGION_FLAT(IMX8MP_HSIO_BLK_CTRL_BASE, IMX8MP_HSIO_BLK_CTRL_SIZE, MT_DEVICE | MT_RW),
    { 0 }
};

int plat_boot_reason(void)
{
    return -PB_ERR_NOT_IMPLEMENTED;
}

const char *plat_boot_reason_str(void)
{
    return "";
}

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    union {
        uint32_t uid[2];
        uint8_t uid_bytes[8];
    } plat_unique;

    if (*length < sizeof(plat_unique.uid_bytes))
        return -PB_ERR_BUF_TOO_SMALL;
    *length = sizeof(plat_unique.uid_bytes);

    imx_ocotp_read(IMX8M_FUSE_UNIQUE_BANK, IMX8M_FUSE_UNIQUE1_WORD, &plat_unique.uid[0]);
    imx_ocotp_read(IMX8M_FUSE_UNIQUE_BANK, IMX8M_FUSE_UNIQUE2_WORD, &plat_unique.uid[1]);
    memcpy(output, plat_unique.uid_bytes, sizeof(plat_unique.uid_bytes));
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

static int plat_mmu_init(void)
{
    /* Configure MMU */
    reset_xlat_tables();

    mmap_add_region(code_start, code_start, code_end - code_start, MT_RO | MT_MEMORY | MT_EXECUTE);

    mmap_add_region(
        data_start, data_start, data_end - data_start, MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(ro_data_start,
                    ro_data_start,
                    ro_data_end - ro_data_start,
                    MT_RO | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(
        stack_start, stack_start, stack_end - stack_start, MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(rw_nox_start,
                    rw_nox_start,
                    rw_nox_end - rw_nox_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    /* Add the rest of the RAM */
    mmap_add_region(
        rw_nox_end, rw_nox_end, BOARD_RAM_END - rw_nox_end, MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add(imx_mmap);

    init_xlat_tables();

    enable_mmu_el3(0);

    return PB_OK;
}

/* The ROM log, AN12853 Tables 11 and 13. It's in the ROM's part of the OCRAM, below pb */
static const struct imx_rom_log_config rom_log_cfg = {
    .pointer = IMX8M_ROM_LOG_POINTER,
    .ram_start = IMX8M_OCRAM_BASE,
    .ram_end = IMX8M_OCRAM_BASE + IMX8M_OCRAM_SIZE,
    .words = 128,
    .defs = IMX_ROM_LOG_DEFS_V2,
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
static void tzasc_enable(void)
{
    const uint32_t bits = GPR10_TZASC_EN | GPR10_TZASC_ID_SWAP_BYPASS;
    const uint32_t locks = GPR10_LOCK_TZASC_EN | GPR10_LOCK_TZASC_ID_SWAP;
    uint32_t gpr10;

    mmio_clrsetbits_32(IMX8M_IOMUXC_GPR10, 0, bits);
    mmio_clrsetbits_32(IMX8M_IOMUXC_GPR10, 0, locks);
    gpr10 = mmio_read_32(IMX8M_IOMUXC_GPR10);

    if ((gpr10 & (bits | locks)) != (bits | locks))
        LOG_ERR("TZASC not enabled, GPR10 %08" PRIx32, gpr10);
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

int plat_early_init(void)
{
    imx_rom_log_save(&rom_log_cfg);
    (void)imx_hab_log_init(&hab_log_cfg);

    /* The WDOGs' power-down counters, whether the watchdog is used or not */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(WDOG_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_WDOG1), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_WDOG2), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_WDOG3), CCGR_CLK_ON_MASK);
    imx_wdog_disable_powerdown(IMX8M_WDOG1_BASE);
    imx_wdog_disable_powerdown(IMX8M_WDOG2_BASE);
    imx_wdog_disable_powerdown(IMX8M_WDOG3_BASE);

#if CONFIG_ENABLE_WATCHDOG
    /* Enable and ungate WDOG1 clock */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(WDOG_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_WDOG1), CCGR_CLK_ON_MASK);
    imx_wdog_init(IMX8M_WDOG1_BASE, CONFIG_WATCHDOG_TIMEOUT);
#endif
    return PB_OK;
}

int plat_init(void)
{
    int rc;

    /* Init fusebox (reading only), its clock gate may be closed */
    mmio_write_32(CCM_CCGR_SET(CCGR_OCOTP), CCGR_CLK_ON_MASK);
    imx_ocotp_init(IMX8M_OCOTP_BASE, IMX8M_FUSE_WORDS_PER_BANK);

    /* Configure systick */
    mmio_write_32(CCM_CCGR_SET(CCGR_GPT1), CCGR_CLK_ON_MASK);
    imx_gpt_init(IMX8M_GPT1_BASE, MHz(24));
    log_time_start();

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

    /* Read SOC variant and version */
    // private.soc_ver_var = mmio_read_32(CCM_ANALOG_DIGPROG);

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

    tzasc_enable();

    rc = imx8m_ddr_init();
    if (rc != PB_OK) {
        LOG_ERR("LPDDR4 init failed (%i)", rc);
        return rc;
    }

    LOG_DBG("LPDDR4 training complete");

    /* Not fatal: without board_setup() command mode can't be used to recover */
    rc = tzasc_open_dram();

    if (rc != PB_OK)
        LOG_ERR("TZASC setup failed (%i)", rc);

    imx_wdog_kick(IMX8M_WDOG1_BASE);
    plat_mmu_init();

    LOG_DBG("Plat init done");
    return PB_OK;
}

int plat_get_boot_source(struct pb_boot_source *src)
{
    (void)src;
    return -PB_ERR_NOT_SUPPORTED;
}

int plat_lockdown(void)
{
    /* Nothing yet: pb doesn't support the 8M's HAB (D-027), and its CSF
     * leaves SRK revocation locked (F-244) */
    return PB_OK;
}
