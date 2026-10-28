/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Synopsys DesignWare DDR4/LPDDR4 multiPHY on the i.MX8M.
 *
 * This PHY is trained by firmware: an embedded microcontroller (the PMU)
 * runs a training image that NXP distributes as binaries (1D and 2D, IMEM
 * and DMEM). The host loads the image, writes a message block with the
 * DRAM parameters, starts the PMU and follows its progress through a
 * mailbox. Afterwards the PHY Init Engine (PIE) image is loaded, which
 * sequences the DFI init and frequency changes.
 *
 * The AMD/Xilinx Zynq UltraScale+ MPSoC uses the same uMCTL2 controller
 * (see umctl2.h), but an older PHY: the DWC DDR multiPHY with a PHY
 * Utility Block (PUB), which trains in hardware when started through PIR
 * and has PGCR/PGSR/DXnGCR registers at 0xFD080000. None of that applies
 * here, and neither does UG1087's DDR_PHY chapter.
 *
 * CSR addressing: the PHY has 16 bit CSRs, addressed by a CSR number. On
 * the i.MX8M CSR n is at DDRPHY_BASE_ADDR + 4 * n. CSR numbers are built
 * as
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

#ifndef INCLUDE_PLAT_IMX8M_DDRPHY_H_
#define INCLUDE_PLAT_IMX8M_DDRPHY_H_

#include <pb/mmio.h>
#include <pb/utils_def.h>
#include <stdint.h>

#define DDRPHY_BASE_ADDR                 0x3c000000
#define DDRPHY_CSR_ADDR(csr)             (DDRPHY_BASE_ADDR + 4 * (csr))

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

static inline void dwc_ddrphy_apb_wr(uint32_t csr, uint32_t val)
{
    mmio_write_32(DDRPHY_CSR_ADDR(csr), val);
}

static inline uint32_t dwc_ddrphy_apb_rd(uint32_t csr)
{
    return mmio_read_32(DDRPHY_CSR_ADDR(csr));
}

#endif // INCLUDE_PLAT_IMX8M_DDRPHY_H_
