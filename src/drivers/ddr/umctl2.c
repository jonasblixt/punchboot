/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 * Copyright 2018-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Synopsys DesignWare uMCTL2 DDR controller: the steps of the DRAM init
 * that only involve the controller. The SoC code calls them in order and
 * does the resets, clocks and the PHY in between (see plat/imx8m/ddr.c).
 *
 * The steps (self refresh by software and the STAT check for it, the
 * SWCTL/DFIMISC handshake around dfi_init_start, leaving self refresh,
 * port enable) are those of NXP's BSD-3-Clause i.MX8M DRAM code in TF-A
 * (plat/imx/imx8m/ddr/dram_retention.c). Some values differ: TF-A writes
 * DFIMISC without ctl_idle_en and dfi_frequency, PWRCTL 0xaa and 0x88
 * where this writes 0xa8 and 0x08, and RFSHCTL3 0 at the end (F-106). The
 * register layout is checked against AMD's MIT licensed ZynqMP FSBL (see
 * drivers/ddr/umctl2.h); no code was taken from it.
 */

#include <drivers/ddr/umctl2.h>
#include <pb/mmio.h>

/* STAT in self refresh entered through PWRCTL.selfref_sw, as in NXP's code */
#define STAT_SELFREF_MASK \
    (STAT_SELFREF_STATE_MASK | STAT_SELFREF_TYPE_MASK | STAT_OPERATING_MODE_MASK)
#define STAT_SELFREF_BY_SW                                                  \
    (UMCTL2_VAL(STAT_SELFREF_STATE, 2) | UMCTL2_VAL(STAT_SELFREF_TYPE, 2) | \
     UMCTL2_VAL(STAT_OPERATING_MODE, 3))

void umctl2_write_cfg(const struct dram_cfg_param *cfg, unsigned int n)
{
    for (unsigned int i = 0; i < n; i++)
        mmio_write_32(cfg[i].reg, cfg[i].val);
}

void umctl2_write_quasi_dynamic(uintptr_t base, uint32_t off, uint32_t val)
{
    mmio_write_32(base + UMCTL2_SWCTL, 0);
    mmio_write_32(base + off, val);
    mmio_write_32(base + UMCTL2_SWCTL, SWCTL_SW_DONE);
}

uint32_t umctl2_dfimisc(uintptr_t base)
{
    uint32_t freq = UMCTL2_GET(MSTR2_TARGET_FREQUENCY, mmio_read_32(base + UMCTL2_MSTR2));

    return UMCTL2_VAL(DFIMISC_DFI_FREQUENCY, freq) | DFIMISC_CTL_IDLE_EN;
}

void umctl2_enter_selfref_sw(uintptr_t base)
{
    mmio_write_32(base + UMCTL2_DBG1, 0);
    mmio_write_32(base + UMCTL2_RFSHCTL3,
                  UMCTL2_VAL(RFSHCTL3_REFRESH_MODE, 1) | RFSHCTL3_DIS_AUTO_REFRESH);
    mmio_write_32(base + UMCTL2_PWRCTL,
                  PWRCTL_DIS_CAM_DRAIN_SELFREF | PWRCTL_SELFREF_SW |
                      PWRCTL_EN_DFI_DRAM_CLK_DISABLE);

    while ((mmio_read_32(base + UMCTL2_STAT) & STAT_SELFREF_MASK) != STAT_SELFREF_BY_SW)
        ;
}

void umctl2_dfi_init(uintptr_t base)
{
    umctl2_write_quasi_dynamic(base, UMCTL2_DFIMISC, umctl2_dfimisc(base) | DFIMISC_DFI_INIT_START);

    while (!(mmio_read_32(base + UMCTL2_DFISTAT) & DFISTAT_DFI_INIT_COMPLETE))
        ;
}

void umctl2_dfi_init_done(uintptr_t base)
{
    /* Stop the DFI init, set dfi_init_complete_en and leave self refresh */
    mmio_write_32(base + UMCTL2_SWCTL, 0);
    mmio_write_32(base + UMCTL2_DFIMISC, umctl2_dfimisc(base));
    mmio_write_32(base + UMCTL2_DFIMISC, umctl2_dfimisc(base) | DFIMISC_DFI_INIT_COMPLETE_EN);
    mmio_write_32(base + UMCTL2_PWRCTL, PWRCTL_EN_DFI_DRAM_CLK_DISABLE);
    mmio_write_32(base + UMCTL2_SWCTL, SWCTL_SW_DONE);

    mmio_write_32(base + UMCTL2_DFIPHYMSTR, DFIPHYMSTR_DFI_PHYMSTR_EN);

    while (!(mmio_read_32(base + UMCTL2_SWSTAT) & SWSTAT_SW_DONE_ACK))
        ;

    mmio_write_32(base + UMCTL2_DFIPHYMSTR, DFIPHYMSTR_DFI_PHYMSTR_EN);

    /* Wait for STAT.operating_mode normal (bits [1:0], as in NXP's code) */
    while ((mmio_read_32(base + UMCTL2_STAT) & 0x3) != STAT_OPERATING_MODE_NORMAL)
        ;

    /* Enable auto refresh */
    mmio_write_32(base + UMCTL2_RFSHCTL3, UMCTL2_VAL(RFSHCTL3_REFRESH_MODE, 1));
}

void umctl2_enable_port(uintptr_t base, unsigned int port)
{
    mmio_write_32(base + UMCTL2_PCTRL(port), PCTRL_PORT_EN);
}
