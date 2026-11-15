/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M's TZASC, a TZC-380 (plat-imx.md 4.2). The SPL takes it out of
 * bypass before DRAM is used and opens the board's DRAM in region 1 (D-088);
 * pb proper reads both back and records what it finds (platform.md 4.3).
 */

#include <drivers/tzc/tzc380.h>
#include <inttypes.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imx8m/imx8m.h>

#include "plat.h"

#ifdef CONFIG_PLAT_IMX8MQ
#define DDR_WINDOW_SIZE IMX8MQ_DDR_WINDOW_SIZE
#else
#define DDR_WINDOW_SIZE IMX8MP_DDR_WINDOW_SIZE
#endif

/* The DDR window's start, where the TZASC counts from */
#define DDR_WINDOW_BASE UINT64_C(0x40000000)

/* The board's DRAM as the TZASC counts it, from the window's start */
#define TZASC_DRAM_BASE ((uint64_t)BOARD_RAM_BASE - DDR_WINDOW_BASE)
#define TZASC_DRAM_SIZE ((uint64_t)BOARD_RAM_END - BOARD_RAM_BASE)

/* One TZC-380 region: a power of two of at least 32 KiB, aligned to its size */
_Static_assert(TZASC_DRAM_SIZE >= SZ_KiB(32) && (TZASC_DRAM_SIZE & (TZASC_DRAM_SIZE - 1)) == 0,
               "The board's DRAM isn't a TZASC region's size");
_Static_assert((TZASC_DRAM_BASE & (TZASC_DRAM_SIZE - 1)) == 0,
               "The board's DRAM isn't aligned to its size, as a TZASC region");
_Static_assert((uint64_t)BOARD_RAM_BASE >= DDR_WINDOW_BASE &&
                   TZASC_DRAM_BASE + TZASC_DRAM_SIZE <= DDR_WINDOW_SIZE,
               "The board's DRAM isn't in the DDR window");

#define GPR10_BITS  (GPR10_TZASC_EN | GPR10_TZASC_ID_SWAP_BYPASS)
#define GPR10_LOCKS (GPR10_LOCK_TZASC_EN | GPR10_LOCK_TZASC_ID_SWAP)

/*
 * The TZASC checks no DRAM access until GPR10's TZASC_EN is set, and TF-A's
 * BL31 only programs it when it is. Set before DRAM is used, with
 * ID_SWAP_BYPASS, which the RM asks for (8MP RM 4.9.1.2), and lock both:
 * the normal world reaches the GPRs. OP-TEE checks all three.
 */
int imx8m_tzasc_enable(void)
{
    uint32_t gpr10;

    mmio_clrsetbits_32(IMX8M_IOMUXC_GPR10, 0, GPR10_BITS);
    mmio_clrsetbits_32(IMX8M_IOMUXC_GPR10, 0, GPR10_LOCKS);
    gpr10 = mmio_read_32(IMX8M_IOMUXC_GPR10);

    if ((gpr10 & (GPR10_BITS | GPR10_LOCKS)) != (GPR10_BITS | GPR10_LOCKS)) {
        LOG_ERR("TZASC not enabled, GPR10 %08" PRIx32, gpr10);
        return -PB_ERR_STATE;
    }

    return PB_OK;
}

/*
 * Out of bypass, region 0 lets only secure accesses through, and some
 * masters are non-secure: the DWC3, command mode's USB, among them, and the
 * boot ROM's own loads of pb proper on the 8MQ. Open the board's DRAM, not
 * the window, to both worlds in region 1 and leave region 0 secure, so that
 * the copies of DRAM in the window stay protected (platform.md 4.3, F-261);
 * OP-TEE replaces region 1 with its own.
 */
int imx8m_tzasc_open_dram(void)
{
    int rc;

    rc = tzc380_init(IMX8M_TZASC_BASE);

    if (rc != PB_OK)
        return rc;

    return tzc380_configure_region(1, TZASC_DRAM_BASE, TZASC_DRAM_SIZE, TZC380_SP_ALL);
}

/*
 * pb proper (D-088): what the SPL set up, read back rather than trusted. The
 * enable and its locks in GPR10, and region 1 over the board's DRAM and no
 * more, to both worlds, with no subregion disabled.
 */
int imx8m_tzasc_check(void)
{
    uint32_t gpr10 = mmio_read_32(IMX8M_IOMUXC_GPR10);
    uint64_t base;
    uint64_t size;
    uint32_t sp;
    int rc;

    if ((gpr10 & (GPR10_BITS | GPR10_LOCKS)) != (GPR10_BITS | GPR10_LOCKS)) {
        LOG_ERR("TZASC not enabled, GPR10 %08" PRIx32, gpr10);
        return -PB_ERR_STATE;
    }

    rc = tzc380_init(IMX8M_TZASC_BASE);

    if (rc != PB_OK)
        return rc;

    rc = tzc380_get_region(1, &base, &size, &sp);

    if (rc != PB_OK)
        return rc;

    if (base != TZASC_DRAM_BASE || size != TZASC_DRAM_SIZE || sp != TZC380_SP_ALL) {
        LOG_ERR("TZASC region 1: base 0x%" PRIx64 ", %" PRIu64 " MiB, sp %08" PRIx32,
                base,
                size >> 20,
                sp);
        return -PB_ERR_STATE;
    }

    return PB_OK;
}
