/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Synopsys DesignWare DDR4/LPDDR4 multiPHY with firmware training.
 *
 * Used on the NXP i.MX8M family (8MQ, 8MM, 8MN, 8MP). The PHY is trained by
 * firmware: an embedded microcontroller (the PMU) runs a training image
 * that the SoC vendor distributes as binaries (1D and 2D, IMEM and DMEM).
 * The host loads the image, writes a message block with the DRAM
 * parameters, starts the PMU and follows its progress through a mailbox.
 * Afterwards the PHY Init Engine (PIE) image is loaded, which sequences the
 * DFI init and frequency changes.
 *
 * Not to be confused with the older DWC DDR multiPHY with a PHY Utility
 * Block (PUB), which trains in hardware when started through PIR and has
 * PGCR/PGSR/DXnGCR registers: that one is in the Zynq UltraScale+ MPSoC
 * (0xFD080000, UG1087 DDR_PHY) and, with Allwinner's own register map, in
 * the Allwinner H3/H5/A64/H6. None of that applies here.
 *
 * CSR addressing: the PHY has 16 bit CSRs, addressed by a CSR number. The
 * SoC maps CSR n at base + stride * n (stride 4 on the i.MX8M). CSR numbers
 * are built as
 *
 *   [23:20] P-state (frequency set point) for per-P-state registers
 *   [19:12] block, see DDRPHY_BLK_* (bits [15:12] select the instance
 *           of ANIB and DBYTE)
 *   [11:0]  register in the block
 *
 * so 0x200b2, 0x1200b2 and 0x2200b2 are the same MASTER0 register for
 * P-states 0, 1 and 2. The CSR and block names follow the Synopsys
 * databook as used in NXP's reference code and DDR tool output.
 */

#ifndef INCLUDE_DRIVERS_DDR_DWC_DDRPHY_H
#define INCLUDE_DRIVERS_DDR_DWC_DDRPHY_H

#include <drivers/ddr/dram_cfg.h>
#include <pb/utils_def.h>
#include <stdint.h>

#define DDRPHY_CSR(pstate, blk, reg)     (((pstate) << 20) | ((blk) << 12) | (reg))

/* Blocks, bits [19:12] of a CSR number */
#define DDRPHY_BLK_ANIB(n)               (0x00 + (n)) /* Address/command nibbles */
#define DDRPHY_BLK_DBYTE(n)              (0x10 + (n)) /* Data bytes */
#define DDRPHY_BLK_MASTER0               0x20
#define DDRPHY_BLK_ACSM0                 0x40 /* Address/command sequence memory */
#define DDRPHY_BLK_IMEM                  0x50 /* PMU instruction memory */
#define DDRPHY_BLK_DMEM                  0x54 /* PMU data memory, message block */
#define DDRPHY_BLK_PPGC0                 0x70
#define DDRPHY_BLK_INITENG0              0x90 /* PHY Init Engine */
#define DDRPHY_BLK_DRTUB0                0xc0
#define DDRPHY_BLK_APBONLY0              0xd0

/* PMU memories. The 32 bit firmware words go into pairs of 16 bit CSRs. */
#define DDRPHY_IMEM                      DDRPHY_CSR(0, DDRPHY_BLK_IMEM, 0)
#define DDRPHY_DMEM                      DDRPHY_CSR(0, DDRPHY_BLK_DMEM, 0)

/* MASTER0 */
#define DDRPHY_CALBUSY                   DDRPHY_CSR(0, DDRPHY_BLK_MASTER0, 0x097)
#define CALBUSY_BUSY                     BIT_32(0) /* Impedance calibration running */

/* DRTUB0 */
#define DDRPHY_UCCLKHCLKENABLES          DDRPHY_CSR(0, DDRPHY_BLK_DRTUB0, 0x080)
#define UCCLKHCLKENABLES_HCLKEN          BIT_32(1)
#define UCCLKHCLKENABLES_UCCLKEN         BIT_32(0)

/* APBONLY0: ownership, reset and mailbox of the PMU */
#define DDRPHY_MICROCONTMUXSEL           DDRPHY_CSR(0, DDRPHY_BLK_APBONLY0, 0x000)
/* 0: the APB (us) can access the CSRs and PMU memories, 1: the PMU */
#define MICROCONTMUXSEL_APB              0
#define MICROCONTMUXSEL_PMU              1

#define DDRPHY_UCTSHADOWREGS             DDRPHY_CSR(0, DDRPHY_BLK_APBONLY0, 0x004)
/* 0: the PMU has posted a message in UctWriteOnlyShadow */
#define UCTSHADOWREGS_UCTWRITEPROTSHADOW BIT_32(0)

#define DDRPHY_DCTWRITEPROT              DDRPHY_CSR(0, DDRPHY_BLK_APBONLY0, 0x031)
/* Write 0 to acknowledge a message, 1 when the PMU has seen the ack */
#define DCTWRITEPROT_ACK                 0
#define DCTWRITEPROT_RELEASE             1

#define DDRPHY_UCTWRITEONLYSHADOW        DDRPHY_CSR(0, DDRPHY_BLK_APBONLY0, 0x032)
#define DDRPHY_UCTDATWRITEONLYSHADOW     DDRPHY_CSR(0, DDRPHY_BLK_APBONLY0, 0x034)

#define DDRPHY_MICRORESET                DDRPHY_CSR(0, DDRPHY_BLK_APBONLY0, 0x099)
#define MICRORESET_RESETTOMICRO          BIT_32(3)
#define MICRORESET_STALLTOMICRO          BIT_32(0)

/* Major messages from the training firmware (UctWriteOnlyShadow) */
#define DDRPHY_MAIL_TRAINING_DONE        0x07
#define DDRPHY_MAIL_STREAMING_MSG        0x08
#define DDRPHY_MAIL_TRAINING_FAILED      0xff

/*
 * Address/command drive strength (ATxImpedance) in the PHY's own encoding,
 * as in NXP's reference code.
 */
#define LPDDR4_PHY_ADDR_RON60            0x1
#define LPDDR4_PHY_ADDR_RON40            0x3
#define LPDDR4_PHY_ADDR_RON30            0x7
#define LPDDR4_PHY_ADDR_RON24            0xf
#define LPDDR4_PHY_ADDR_RON20            0x1f

/* PMU: held in reset, stalled, running */
#define MICRORESET_HOLD                  (MICRORESET_RESETTOMICRO | MICRORESET_STALLTOMICRO)
#define MICRORESET_STALL                 MICRORESET_STALLTOMICRO
#define MICRORESET_RUN                   0

/* API ------------------------------------------------------------------- */

/* A training firmware image, IMEM and DMEM parts */
struct dwc_ddrphy_fw {
    const uint32_t *imem;
    const uint32_t *imem_end;
    const uint32_t *dmem;
    const uint32_t *dmem_end;
};

struct dwc_ddrphy {
    uintptr_t base;
    unsigned int csr_stride; /* Bytes between CSR n and n + 1 */
    const struct dwc_ddrphy_fw *fw[2]; /* Indexed by enum fw_type */
};

void dwc_ddrphy_write(const struct dwc_ddrphy *phy, uint32_t csr, uint32_t val);
uint32_t dwc_ddrphy_read(const struct dwc_ddrphy *phy, uint32_t csr);

/* Write a table of (CSR, value) pairs */
void dwc_ddrphy_write_cfg(const struct dwc_ddrphy *phy,
                          const struct dram_cfg_param *cfg,
                          unsigned int n);

/*
 * One training run: load the firmware and the message block, run the PMU
 * and wait for its result. The DRAM clock must already be set for
 * fsp->drate. Returns -PB_ERR_IO if the firmware doesn't read back or the
 * training fails.
 */
int dwc_ddrphy_train(const struct dwc_ddrphy *phy, const struct dram_fsp_msg *fsp);

/* Wait for the impedance calibration (after loading the PIE image) */
void dwc_ddrphy_wait_calibration(const struct dwc_ddrphy *phy);

/* Read back CSRs, e.g. the trained ones, into csrs[].val */
void dwc_ddrphy_read_csrs(const struct dwc_ddrphy *phy,
                          struct dram_cfg_param *csrs,
                          unsigned int n);

#endif // INCLUDE_DRIVERS_DDR_DWC_DDRPHY_H
