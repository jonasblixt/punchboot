/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Synopsys DesignWare uMCTL2 DDR memory controller (DDRC) on the i.MX8M.
 *
 * The same controller IP is the PS DDR controller of the AMD/Xilinx Zynq
 * UltraScale+ MPSoC/RFSoC ("DDRC" at 0xFD070000 in UG1087,
 * "xlnx,zynqmp-ddrc-2.40a"). The register offsets and the fields below
 * match the ZynqMP register map. Fields marked [i.MX8M] are not programmed
 * by the ZynqMP FSBL and come from the i.MX8M reference manual and NXP's
 * reference code. Note that the PHY is a different product on the two
 * SoCs, see ddrphy.h.
 *
 * Register naming follows the Synopsys databook, as used by both vendors:
 * DDRC_<register>(n) is the address of the register in controller n. Only
 * controller 0 exists on the i.MX8MQ. The board timing tables use these
 * names, so they stay as they are.
 */

#ifndef PLAT_IMX8M_UMCTL2_H_
#define PLAT_IMX8M_UMCTL2_H_

#include <pb/utils_def.h>

#define DDRC_IPS_BASE_ADDR(X)          (0x3d400000 + ((X) * 0x2000000))
#define DDRC_REG(X, off)               (DDRC_IPS_BASE_ADDR(X) + (off))

/* Insert value 'v' into the field 'f' (f##_MASK, f##_SHIFT) */
#define UMCTL2_VAL(f, v)               ((((uint32_t)(v)) << f##_SHIFT) & f##_MASK)
#define UMCTL2_GET(f, reg)             ((((uint32_t)(reg)) & f##_MASK) >> f##_SHIFT)

/* Register map --------------------------------------------------------- */

#define DDRC_MSTR(X)                   DDRC_REG(X, 0x000) /* Master */
#define DDRC_STAT(X)                   DDRC_REG(X, 0x004) /* Operating mode status */
#define DDRC_MSTR1(X)                  DDRC_REG(X, 0x008)
#define DDRC_MRCTRL0(X)                DDRC_REG(X, 0x010) /* Mode register read/write control */
#define DDRC_MRCTRL1(X)                DDRC_REG(X, 0x014)
#define DDRC_MRSTAT(X)                 DDRC_REG(X, 0x018)
#define DDRC_MRCTRL2(X)                DDRC_REG(X, 0x01c)
#define DDRC_DERATEEN(X)               DDRC_REG(X, 0x020) /* LPDDR temperature derating */
#define DDRC_DERATEINT(X)              DDRC_REG(X, 0x024)
#define DDRC_MSTR2(X)                  DDRC_REG(X, 0x028) /* Target frequency (FREQ0..3) */
#define DDRC_PWRCTL(X)                 DDRC_REG(X, 0x030) /* Low power control */
#define DDRC_PWRTMG(X)                 DDRC_REG(X, 0x034)
#define DDRC_HWLPCTL(X)                DDRC_REG(X, 0x038)
#define DDRC_HWFFCCTL(X)               DDRC_REG(X, 0x03c) /* Hardware fast frequency change */
#define DDRC_HWFFCSTAT(X)              DDRC_REG(X, 0x040)
#define DDRC_RFSHCTL0(X)               DDRC_REG(X, 0x050) /* Refresh control */
#define DDRC_RFSHCTL1(X)               DDRC_REG(X, 0x054)
#define DDRC_RFSHCTL2(X)               DDRC_REG(X, 0x058)
#define DDRC_RFSHCTL3(X)               DDRC_REG(X, 0x060)
#define DDRC_RFSHTMG(X)                DDRC_REG(X, 0x064)
#define DDRC_ECCCFG0(X)                DDRC_REG(X, 0x070) /* ECC */
#define DDRC_ECCCFG1(X)                DDRC_REG(X, 0x074)
#define DDRC_ECCSTAT(X)                DDRC_REG(X, 0x078)
#define DDRC_ECCCLR(X)                 DDRC_REG(X, 0x07c)
#define DDRC_ECCERRCNT(X)              DDRC_REG(X, 0x080)
#define DDRC_ECCCADDR0(X)              DDRC_REG(X, 0x084)
#define DDRC_ECCCADDR1(X)              DDRC_REG(X, 0x088)
#define DDRC_ECCCSYN0(X)               DDRC_REG(X, 0x08c)
#define DDRC_ECCCSYN1(X)               DDRC_REG(X, 0x090)
#define DDRC_ECCCSYN2(X)               DDRC_REG(X, 0x094)
#define DDRC_ECCBITMASK0(X)            DDRC_REG(X, 0x098)
#define DDRC_ECCBITMASK1(X)            DDRC_REG(X, 0x09c)
#define DDRC_ECCBITMASK2(X)            DDRC_REG(X, 0x0a0)
#define DDRC_ECCUADDR0(X)              DDRC_REG(X, 0x0a4)
#define DDRC_ECCUADDR1(X)              DDRC_REG(X, 0x0a8)
#define DDRC_ECCUSYN0(X)               DDRC_REG(X, 0x0ac)
#define DDRC_ECCUSYN1(X)               DDRC_REG(X, 0x0b0)
#define DDRC_ECCUSYN2(X)               DDRC_REG(X, 0x0b4)
#define DDRC_ECCPOISONADDR0(X)         DDRC_REG(X, 0x0b8)
#define DDRC_ECCPOISONADDR1(X)         DDRC_REG(X, 0x0bc)
#define DDRC_CRCPARCTL0(X)             DDRC_REG(X, 0x0c0) /* DDR4 CRC/parity */
#define DDRC_CRCPARCTL1(X)             DDRC_REG(X, 0x0c4)
#define DDRC_CRCPARCTL2(X)             DDRC_REG(X, 0x0c8)
#define DDRC_CRCPARSTAT(X)             DDRC_REG(X, 0x0cc)
#define DDRC_INIT0(X)                  DDRC_REG(X, 0x0d0) /* SDRAM initialization */
#define DDRC_INIT1(X)                  DDRC_REG(X, 0x0d4)
#define DDRC_INIT2(X)                  DDRC_REG(X, 0x0d8)
#define DDRC_INIT3(X)                  DDRC_REG(X, 0x0dc)
#define DDRC_INIT4(X)                  DDRC_REG(X, 0x0e0)
#define DDRC_INIT5(X)                  DDRC_REG(X, 0x0e4)
#define DDRC_INIT6(X)                  DDRC_REG(X, 0x0e8)
#define DDRC_INIT7(X)                  DDRC_REG(X, 0x0ec)
#define DDRC_DIMMCTL(X)                DDRC_REG(X, 0x0f0)
#define DDRC_RANKCTL(X)                DDRC_REG(X, 0x0f4)
#define DDRC_DRAMTMG0(X)               DDRC_REG(X, 0x100) /* SDRAM timing */
#define DDRC_DRAMTMG1(X)               DDRC_REG(X, 0x104)
#define DDRC_DRAMTMG2(X)               DDRC_REG(X, 0x108)
#define DDRC_DRAMTMG3(X)               DDRC_REG(X, 0x10c)
#define DDRC_DRAMTMG4(X)               DDRC_REG(X, 0x110)
#define DDRC_DRAMTMG5(X)               DDRC_REG(X, 0x114)
#define DDRC_DRAMTMG6(X)               DDRC_REG(X, 0x118)
#define DDRC_DRAMTMG7(X)               DDRC_REG(X, 0x11c)
#define DDRC_DRAMTMG8(X)               DDRC_REG(X, 0x120)
#define DDRC_DRAMTMG9(X)               DDRC_REG(X, 0x124)
#define DDRC_DRAMTMG10(X)              DDRC_REG(X, 0x128)
#define DDRC_DRAMTMG11(X)              DDRC_REG(X, 0x12c)
#define DDRC_DRAMTMG12(X)              DDRC_REG(X, 0x130)
#define DDRC_DRAMTMG13(X)              DDRC_REG(X, 0x134)
#define DDRC_DRAMTMG14(X)              DDRC_REG(X, 0x138)
#define DDRC_DRAMTMG15(X)              DDRC_REG(X, 0x13c)
#define DDRC_DRAMTMG16(X)              DDRC_REG(X, 0x140)
#define DDRC_DRAMTMG17(X)              DDRC_REG(X, 0x144)
#define DDRC_ZQCTL0(X)                 DDRC_REG(X, 0x180) /* ZQ calibration */
#define DDRC_ZQCTL1(X)                 DDRC_REG(X, 0x184)
#define DDRC_ZQCTL2(X)                 DDRC_REG(X, 0x188)
#define DDRC_ZQSTAT(X)                 DDRC_REG(X, 0x18c)
#define DDRC_DFITMG0(X)                DDRC_REG(X, 0x190) /* DFI (controller <-> PHY) timing */
#define DDRC_DFITMG1(X)                DDRC_REG(X, 0x194)
#define DDRC_DFILPCFG0(X)              DDRC_REG(X, 0x198)
#define DDRC_DFILPCFG1(X)              DDRC_REG(X, 0x19c)
#define DDRC_DFIUPD0(X)                DDRC_REG(X, 0x1a0)
#define DDRC_DFIUPD1(X)                DDRC_REG(X, 0x1a4)
#define DDRC_DFIUPD2(X)                DDRC_REG(X, 0x1a8)
#define DDRC_DFIMISC(X)                DDRC_REG(X, 0x1b0)
#define DDRC_DFITMG2(X)                DDRC_REG(X, 0x1b4)
#define DDRC_DFITMG3(X)                DDRC_REG(X, 0x1b8)
#define DDRC_DFISTAT(X)                DDRC_REG(X, 0x1bc)
#define DDRC_DBICTL(X)                 DDRC_REG(X, 0x1c0)
#define DDRC_DFIPHYMSTR(X)             DDRC_REG(X, 0x1c4)
#define DDRC_TRAINCTL0(X)              DDRC_REG(X, 0x1d0)
#define DDRC_TRAINCTL1(X)              DDRC_REG(X, 0x1d4)
#define DDRC_TRAINCTL2(X)              DDRC_REG(X, 0x1d8)
#define DDRC_TRAINSTAT(X)              DDRC_REG(X, 0x1dc)
#define DDRC_ADDRMAP0(X)               DDRC_REG(X, 0x200) /* HIF address -> SDRAM address */
#define DDRC_ADDRMAP1(X)               DDRC_REG(X, 0x204)
#define DDRC_ADDRMAP2(X)               DDRC_REG(X, 0x208)
#define DDRC_ADDRMAP3(X)               DDRC_REG(X, 0x20c)
#define DDRC_ADDRMAP4(X)               DDRC_REG(X, 0x210)
#define DDRC_ADDRMAP5(X)               DDRC_REG(X, 0x214)
#define DDRC_ADDRMAP6(X)               DDRC_REG(X, 0x218)
#define DDRC_ADDRMAP7(X)               DDRC_REG(X, 0x21c)
#define DDRC_ADDRMAP8(X)               DDRC_REG(X, 0x220)
#define DDRC_ADDRMAP9(X)               DDRC_REG(X, 0x224)
#define DDRC_ADDRMAP10(X)              DDRC_REG(X, 0x228)
#define DDRC_ADDRMAP11(X)              DDRC_REG(X, 0x22c)
#define DDRC_ODTCFG(X)                 DDRC_REG(X, 0x240)
#define DDRC_ODTMAP(X)                 DDRC_REG(X, 0x244)
#define DDRC_SCHED(X)                  DDRC_REG(X, 0x250) /* Scheduler */
#define DDRC_SCHED1(X)                 DDRC_REG(X, 0x254)
#define DDRC_PERFHPR1(X)               DDRC_REG(X, 0x25c)
#define DDRC_PERFLPR1(X)               DDRC_REG(X, 0x264)
#define DDRC_PERFWR1(X)                DDRC_REG(X, 0x26c)
#define DDRC_PERFVPR1(X)               DDRC_REG(X, 0x274)
#define DDRC_PERFVPW1(X)               DDRC_REG(X, 0x278)
#define DDRC_DQMAP0(X)                 DDRC_REG(X, 0x280)
#define DDRC_DQMAP1(X)                 DDRC_REG(X, 0x284)
#define DDRC_DQMAP2(X)                 DDRC_REG(X, 0x288)
#define DDRC_DQMAP3(X)                 DDRC_REG(X, 0x28c)
#define DDRC_DQMAP4(X)                 DDRC_REG(X, 0x290)
#define DDRC_DQMAP5(X)                 DDRC_REG(X, 0x294)
#define DDRC_DBG0(X)                   DDRC_REG(X, 0x300) /* Debug */
#define DDRC_DBG1(X)                   DDRC_REG(X, 0x304)
#define DDRC_DBGCAM(X)                 DDRC_REG(X, 0x308)
#define DDRC_DBGCMD(X)                 DDRC_REG(X, 0x30c)
#define DDRC_DBGSTAT(X)                DDRC_REG(X, 0x310)
#define DDRC_SWCTL(X)                  DDRC_REG(X, 0x320) /* Quasi-dynamic register programming */
#define DDRC_SWSTAT(X)                 DDRC_REG(X, 0x324)
#define DDRC_OCPARCFG0(X)              DDRC_REG(X, 0x330) /* On-chip parity */
#define DDRC_OCPARCFG1(X)              DDRC_REG(X, 0x334)
#define DDRC_OCPARCFG2(X)              DDRC_REG(X, 0x338)
#define DDRC_OCPARCFG3(X)              DDRC_REG(X, 0x33c)
#define DDRC_OCPARSTAT0(X)             DDRC_REG(X, 0x340)
#define DDRC_OCPARSTAT1(X)             DDRC_REG(X, 0x344)
#define DDRC_OCPARWLOG0(X)             DDRC_REG(X, 0x348)
#define DDRC_OCPARWLOG1(X)             DDRC_REG(X, 0x34c)
#define DDRC_OCPARWLOG2(X)             DDRC_REG(X, 0x350)
#define DDRC_OCPARAWLOG0(X)            DDRC_REG(X, 0x354)
#define DDRC_OCPARAWLOG1(X)            DDRC_REG(X, 0x358)
#define DDRC_OCPARRLOG0(X)             DDRC_REG(X, 0x35c)
#define DDRC_OCPARRLOG1(X)             DDRC_REG(X, 0x360)
#define DDRC_OCPARARLOG0(X)            DDRC_REG(X, 0x364)
#define DDRC_OCPARARLOG1(X)            DDRC_REG(X, 0x368)
#define DDRC_POISONCFG(X)              DDRC_REG(X, 0x36c)
#define DDRC_POISONSTAT(X)             DDRC_REG(X, 0x370)

/*
 * Multiport (AXI) registers. Port n's registers are at 0xb0 * n from
 * port 0's (PCTRL_1 is at 0x540 on both SoCs).
 */
#define DDRC_PORT_STRIDE               0xb0
#define DDRC_PSTAT(X)                  DDRC_REG(X, 0x3fc)
#define DDRC_PCCFG(X)                  DDRC_REG(X, 0x400)
#define DDRC_PCFGR(X, n)               DDRC_REG(X, 0x404 + (n) * DDRC_PORT_STRIDE)
#define DDRC_PCFGW(X, n)               DDRC_REG(X, 0x408 + (n) * DDRC_PORT_STRIDE)
#define DDRC_PCFGC(X, n)               DDRC_REG(X, 0x40c + (n) * DDRC_PORT_STRIDE)
#define DDRC_PCFGIDMASKCH(X)           DDRC_REG(X, 0x410)
#define DDRC_PCFGIDVALUECH(X)          DDRC_REG(X, 0x414)
#define DDRC_PCTRL(X, n)               DDRC_REG(X, 0x490 + (n) * DDRC_PORT_STRIDE)
#define DDRC_PCFGQOS0(X, n)            DDRC_REG(X, 0x494 + (n) * DDRC_PORT_STRIDE)
#define DDRC_PCFGQOS1(X, n)            DDRC_REG(X, 0x498 + (n) * DDRC_PORT_STRIDE)
#define DDRC_PCFGWQOS0(X, n)           DDRC_REG(X, 0x49c + (n) * DDRC_PORT_STRIDE)
#define DDRC_PCFGWQOS1(X, n)           DDRC_REG(X, 0x4a0 + (n) * DDRC_PORT_STRIDE)

/* Port 0, the only port on the i.MX8MQ */
#define DDRC_PCFGR_0(X)                DDRC_PCFGR(X, 0)
#define DDRC_PCFGW_0(X)                DDRC_PCFGW(X, 0)
#define DDRC_PCFGC_0(X)                DDRC_PCFGC(X, 0)
#define DDRC_PCTRL_0(X)                DDRC_PCTRL(X, 0)
#define DDRC_PCFGQOS0_0(X)             DDRC_PCFGQOS0(X, 0)
#define DDRC_PCFGQOS1_0(X)             DDRC_PCFGQOS1(X, 0)
#define DDRC_PCFGWQOS0_0(X)            DDRC_PCFGWQOS0(X, 0)
#define DDRC_PCFGWQOS1_0(X)            DDRC_PCFGWQOS1(X, 0)

#define DDRC_SARBASE0(X)               DDRC_REG(X, 0xf04) /* System address regions */
#define DDRC_SARSIZE0(X)               DDRC_REG(X, 0xf08)
#define DDRC_SBRCTL(X)                 DDRC_REG(X, 0xf24) /* Scrubber */
#define DDRC_SBRSTAT(X)                DDRC_REG(X, 0xf28)
#define DDRC_SBRWDATA0(X)              DDRC_REG(X, 0xf2c)
#define DDRC_SBRWDATA1(X)              DDRC_REG(X, 0xf30)
#define DDRC_PDCH(X)                   DDRC_REG(X, 0xf34)

/*
 * Frequency set point shadow registers. The timing registers of FREQ1..3
 * (selected by MSTR2.target_frequency) are copies of the FREQ0 registers
 * above at 0x1000 * (n + 1). Only these registers have shadows.
 */
#define DDRC_FREQ_OFFSET(n)            (0x1000 * ((n) + 1))
#define DDRC_FREQ(n, reg)              ((reg) + DDRC_FREQ_OFFSET(n))

#define DDRC_FREQ1_DERATEEN(X)         DDRC_FREQ(1, DDRC_DERATEEN(X))
#define DDRC_FREQ1_DERATEINT(X)        DDRC_FREQ(1, DDRC_DERATEINT(X))
#define DDRC_FREQ1_RFSHCTL0(X)         DDRC_FREQ(1, DDRC_RFSHCTL0(X))
#define DDRC_FREQ1_RFSHTMG(X)          DDRC_FREQ(1, DDRC_RFSHTMG(X))
#define DDRC_FREQ1_INIT3(X)            DDRC_FREQ(1, DDRC_INIT3(X))
#define DDRC_FREQ1_INIT4(X)            DDRC_FREQ(1, DDRC_INIT4(X))
#define DDRC_FREQ1_INIT6(X)            DDRC_FREQ(1, DDRC_INIT6(X))
#define DDRC_FREQ1_INIT7(X)            DDRC_FREQ(1, DDRC_INIT7(X))
#define DDRC_FREQ1_DRAMTMG0(X)         DDRC_FREQ(1, DDRC_DRAMTMG0(X))
#define DDRC_FREQ1_DRAMTMG1(X)         DDRC_FREQ(1, DDRC_DRAMTMG1(X))
#define DDRC_FREQ1_DRAMTMG2(X)         DDRC_FREQ(1, DDRC_DRAMTMG2(X))
#define DDRC_FREQ1_DRAMTMG3(X)         DDRC_FREQ(1, DDRC_DRAMTMG3(X))
#define DDRC_FREQ1_DRAMTMG4(X)         DDRC_FREQ(1, DDRC_DRAMTMG4(X))
#define DDRC_FREQ1_DRAMTMG5(X)         DDRC_FREQ(1, DDRC_DRAMTMG5(X))
#define DDRC_FREQ1_DRAMTMG6(X)         DDRC_FREQ(1, DDRC_DRAMTMG6(X))
#define DDRC_FREQ1_DRAMTMG7(X)         DDRC_FREQ(1, DDRC_DRAMTMG7(X))
#define DDRC_FREQ1_DRAMTMG8(X)         DDRC_FREQ(1, DDRC_DRAMTMG8(X))
#define DDRC_FREQ1_DRAMTMG9(X)         DDRC_FREQ(1, DDRC_DRAMTMG9(X))
#define DDRC_FREQ1_DRAMTMG10(X)        DDRC_FREQ(1, DDRC_DRAMTMG10(X))
#define DDRC_FREQ1_DRAMTMG11(X)        DDRC_FREQ(1, DDRC_DRAMTMG11(X))
#define DDRC_FREQ1_DRAMTMG12(X)        DDRC_FREQ(1, DDRC_DRAMTMG12(X))
#define DDRC_FREQ1_DRAMTMG13(X)        DDRC_FREQ(1, DDRC_DRAMTMG13(X))
#define DDRC_FREQ1_DRAMTMG14(X)        DDRC_FREQ(1, DDRC_DRAMTMG14(X))
#define DDRC_FREQ1_DRAMTMG15(X)        DDRC_FREQ(1, DDRC_DRAMTMG15(X))
#define DDRC_FREQ1_DRAMTMG16(X)        DDRC_FREQ(1, DDRC_DRAMTMG16(X))
#define DDRC_FREQ1_DRAMTMG17(X)        DDRC_FREQ(1, DDRC_DRAMTMG17(X))
#define DDRC_FREQ1_ZQCTL0(X)           DDRC_FREQ(1, DDRC_ZQCTL0(X))
#define DDRC_FREQ1_DFITMG0(X)          DDRC_FREQ(1, DDRC_DFITMG0(X))
#define DDRC_FREQ1_DFITMG1(X)          DDRC_FREQ(1, DDRC_DFITMG1(X))
#define DDRC_FREQ1_DFITMG2(X)          DDRC_FREQ(1, DDRC_DFITMG2(X))
#define DDRC_FREQ1_DFITMG3(X)          DDRC_FREQ(1, DDRC_DFITMG3(X))
#define DDRC_FREQ1_ODTCFG(X)           DDRC_FREQ(1, DDRC_ODTCFG(X))

#define DDRC_FREQ2_DERATEEN(X)         DDRC_FREQ(2, DDRC_DERATEEN(X))
#define DDRC_FREQ2_DERATEINT(X)        DDRC_FREQ(2, DDRC_DERATEINT(X))
#define DDRC_FREQ2_RFSHCTL0(X)         DDRC_FREQ(2, DDRC_RFSHCTL0(X))
#define DDRC_FREQ2_RFSHTMG(X)          DDRC_FREQ(2, DDRC_RFSHTMG(X))
#define DDRC_FREQ2_INIT3(X)            DDRC_FREQ(2, DDRC_INIT3(X))
#define DDRC_FREQ2_INIT4(X)            DDRC_FREQ(2, DDRC_INIT4(X))
#define DDRC_FREQ2_INIT6(X)            DDRC_FREQ(2, DDRC_INIT6(X))
#define DDRC_FREQ2_INIT7(X)            DDRC_FREQ(2, DDRC_INIT7(X))
#define DDRC_FREQ2_DRAMTMG0(X)         DDRC_FREQ(2, DDRC_DRAMTMG0(X))
#define DDRC_FREQ2_DRAMTMG1(X)         DDRC_FREQ(2, DDRC_DRAMTMG1(X))
#define DDRC_FREQ2_DRAMTMG2(X)         DDRC_FREQ(2, DDRC_DRAMTMG2(X))
#define DDRC_FREQ2_DRAMTMG3(X)         DDRC_FREQ(2, DDRC_DRAMTMG3(X))
#define DDRC_FREQ2_DRAMTMG4(X)         DDRC_FREQ(2, DDRC_DRAMTMG4(X))
#define DDRC_FREQ2_DRAMTMG5(X)         DDRC_FREQ(2, DDRC_DRAMTMG5(X))
#define DDRC_FREQ2_DRAMTMG6(X)         DDRC_FREQ(2, DDRC_DRAMTMG6(X))
#define DDRC_FREQ2_DRAMTMG7(X)         DDRC_FREQ(2, DDRC_DRAMTMG7(X))
#define DDRC_FREQ2_DRAMTMG8(X)         DDRC_FREQ(2, DDRC_DRAMTMG8(X))
#define DDRC_FREQ2_DRAMTMG9(X)         DDRC_FREQ(2, DDRC_DRAMTMG9(X))
#define DDRC_FREQ2_DRAMTMG10(X)        DDRC_FREQ(2, DDRC_DRAMTMG10(X))
#define DDRC_FREQ2_DRAMTMG11(X)        DDRC_FREQ(2, DDRC_DRAMTMG11(X))
#define DDRC_FREQ2_DRAMTMG12(X)        DDRC_FREQ(2, DDRC_DRAMTMG12(X))
#define DDRC_FREQ2_DRAMTMG13(X)        DDRC_FREQ(2, DDRC_DRAMTMG13(X))
#define DDRC_FREQ2_DRAMTMG14(X)        DDRC_FREQ(2, DDRC_DRAMTMG14(X))
#define DDRC_FREQ2_DRAMTMG15(X)        DDRC_FREQ(2, DDRC_DRAMTMG15(X))
#define DDRC_FREQ2_DRAMTMG16(X)        DDRC_FREQ(2, DDRC_DRAMTMG16(X))
#define DDRC_FREQ2_DRAMTMG17(X)        DDRC_FREQ(2, DDRC_DRAMTMG17(X))
#define DDRC_FREQ2_ZQCTL0(X)           DDRC_FREQ(2, DDRC_ZQCTL0(X))
#define DDRC_FREQ2_DFITMG0(X)          DDRC_FREQ(2, DDRC_DFITMG0(X))
#define DDRC_FREQ2_DFITMG1(X)          DDRC_FREQ(2, DDRC_DFITMG1(X))
#define DDRC_FREQ2_DFITMG2(X)          DDRC_FREQ(2, DDRC_DFITMG2(X))
#define DDRC_FREQ2_DFITMG3(X)          DDRC_FREQ(2, DDRC_DFITMG3(X))
#define DDRC_FREQ2_ODTCFG(X)           DDRC_FREQ(2, DDRC_ODTCFG(X))

#define DDRC_FREQ3_DERATEEN(X)         DDRC_FREQ(3, DDRC_DERATEEN(X))
#define DDRC_FREQ3_DERATEINT(X)        DDRC_FREQ(3, DDRC_DERATEINT(X))
#define DDRC_FREQ3_RFSHCTL0(X)         DDRC_FREQ(3, DDRC_RFSHCTL0(X))
#define DDRC_FREQ3_RFSHTMG(X)          DDRC_FREQ(3, DDRC_RFSHTMG(X))
#define DDRC_FREQ3_INIT3(X)            DDRC_FREQ(3, DDRC_INIT3(X))
#define DDRC_FREQ3_INIT4(X)            DDRC_FREQ(3, DDRC_INIT4(X))
#define DDRC_FREQ3_INIT6(X)            DDRC_FREQ(3, DDRC_INIT6(X))
#define DDRC_FREQ3_INIT7(X)            DDRC_FREQ(3, DDRC_INIT7(X))
#define DDRC_FREQ3_DRAMTMG0(X)         DDRC_FREQ(3, DDRC_DRAMTMG0(X))
#define DDRC_FREQ3_DRAMTMG1(X)         DDRC_FREQ(3, DDRC_DRAMTMG1(X))
#define DDRC_FREQ3_DRAMTMG2(X)         DDRC_FREQ(3, DDRC_DRAMTMG2(X))
#define DDRC_FREQ3_DRAMTMG3(X)         DDRC_FREQ(3, DDRC_DRAMTMG3(X))
#define DDRC_FREQ3_DRAMTMG4(X)         DDRC_FREQ(3, DDRC_DRAMTMG4(X))
#define DDRC_FREQ3_DRAMTMG5(X)         DDRC_FREQ(3, DDRC_DRAMTMG5(X))
#define DDRC_FREQ3_DRAMTMG6(X)         DDRC_FREQ(3, DDRC_DRAMTMG6(X))
#define DDRC_FREQ3_DRAMTMG7(X)         DDRC_FREQ(3, DDRC_DRAMTMG7(X))
#define DDRC_FREQ3_DRAMTMG8(X)         DDRC_FREQ(3, DDRC_DRAMTMG8(X))
#define DDRC_FREQ3_DRAMTMG9(X)         DDRC_FREQ(3, DDRC_DRAMTMG9(X))
#define DDRC_FREQ3_DRAMTMG10(X)        DDRC_FREQ(3, DDRC_DRAMTMG10(X))
#define DDRC_FREQ3_DRAMTMG11(X)        DDRC_FREQ(3, DDRC_DRAMTMG11(X))
#define DDRC_FREQ3_DRAMTMG12(X)        DDRC_FREQ(3, DDRC_DRAMTMG12(X))
#define DDRC_FREQ3_DRAMTMG13(X)        DDRC_FREQ(3, DDRC_DRAMTMG13(X))
#define DDRC_FREQ3_DRAMTMG14(X)        DDRC_FREQ(3, DDRC_DRAMTMG14(X))
#define DDRC_FREQ3_DRAMTMG15(X)        DDRC_FREQ(3, DDRC_DRAMTMG15(X))
#define DDRC_FREQ3_DRAMTMG16(X)        DDRC_FREQ(3, DDRC_DRAMTMG16(X))
#define DDRC_FREQ3_DRAMTMG17(X)        DDRC_FREQ(3, DDRC_DRAMTMG17(X))
#define DDRC_FREQ3_ZQCTL0(X)           DDRC_FREQ(3, DDRC_ZQCTL0(X))
#define DDRC_FREQ3_DFITMG0(X)          DDRC_FREQ(3, DDRC_DFITMG0(X))
#define DDRC_FREQ3_DFITMG1(X)          DDRC_FREQ(3, DDRC_DFITMG1(X))
#define DDRC_FREQ3_DFITMG2(X)          DDRC_FREQ(3, DDRC_DFITMG2(X))
#define DDRC_FREQ3_DFITMG3(X)          DDRC_FREQ(3, DDRC_DFITMG3(X))
#define DDRC_FREQ3_ODTCFG(X)           DDRC_FREQ(3, DDRC_ODTCFG(X))

/* Register fields ------------------------------------------------------- */

/* MSTR */
#define MSTR_DEVICE_CONFIG_MASK        GENMASK_32(31, 30) /* 0: x4, 1: x8, 2: x16, 3: x32 */
#define MSTR_DEVICE_CONFIG_SHIFT       30
#define MSTR_FREQUENCY_MODE            BIT_32(29) /* Enable FREQ1..3 (MSTR2) */
#define MSTR_ACTIVE_RANKS_MASK         GENMASK_32(25, 24) /* 1: rank 0, 3: ranks 0 and 1 */
#define MSTR_ACTIVE_RANKS_SHIFT        24
#define MSTR_BURST_RDWR_MASK           GENMASK_32(19, 16) /* Burst length / 2 */
#define MSTR_BURST_RDWR_SHIFT          16
#define MSTR_DLL_OFF_MODE              BIT_32(15)
#define MSTR_DATA_BUS_WIDTH_MASK       GENMASK_32(13, 12) /* 0: full, 1: half, 2: quarter */
#define MSTR_DATA_BUS_WIDTH_SHIFT      12
#define MSTR_GEARDOWN_MODE             BIT_32(11)
#define MSTR_EN_2T_TIMING_MODE         BIT_32(10)
#define MSTR_BURSTCHOP                 BIT_32(9)
#define MSTR_LPDDR4                    BIT_32(5)
#define MSTR_DDR4                      BIT_32(4)
#define MSTR_LPDDR3                    BIT_32(3)
#define MSTR_LPDDR2                    BIT_32(2)
#define MSTR_DDR3                      BIT_32(0)

/* STAT [i.MX8M] */
#define STAT_SELFREF_STATE_MASK        GENMASK_32(9, 8)
#define STAT_SELFREF_STATE_SHIFT       8
#define STAT_SELFREF_TYPE_MASK         GENMASK_32(5, 4)
#define STAT_SELFREF_TYPE_SHIFT        4
#define STAT_OPERATING_MODE_MASK       GENMASK_32(2, 0)
#define STAT_OPERATING_MODE_SHIFT      0
#define STAT_OPERATING_MODE_NORMAL     1

/* MRCTRL0 */
#define MRCTRL0_MR_WR                  BIT_32(31)
#define MRCTRL0_MR_ADDR_MASK           GENMASK_32(15, 12)
#define MRCTRL0_MR_ADDR_SHIFT          12
#define MRCTRL0_MR_RANK_MASK           GENMASK_32(5, 4)
#define MRCTRL0_MR_RANK_SHIFT          4
#define MRCTRL0_PDA_EN                 BIT_32(2)
#define MRCTRL0_MPR_EN                 BIT_32(1)
#define MRCTRL0_MR_TYPE                BIT_32(0) /* 0: write, 1: read */

/* DERATEEN */
#define DERATEEN_RC_DERATE_VALUE_MASK  GENMASK_32(9, 8)
#define DERATEEN_RC_DERATE_VALUE_SHIFT 8
#define DERATEEN_DERATE_BYTE_MASK      GENMASK_32(7, 4)
#define DERATEEN_DERATE_BYTE_SHIFT     4
#define DERATEEN_DERATE_VALUE          BIT_32(1)
#define DERATEEN_DERATE_ENABLE         BIT_32(0)

/* MSTR2 */
#define MSTR2_TARGET_FREQUENCY_MASK    GENMASK_32(1, 0)
#define MSTR2_TARGET_FREQUENCY_SHIFT   0

/* PWRCTL */
#define PWRCTL_DIS_CAM_DRAIN_SELFREF   BIT_32(7) /* [i.MX8M] */
#define PWRCTL_STAY_IN_SELFREF         BIT_32(6)
#define PWRCTL_SELFREF_SW              BIT_32(5) /* Enter self refresh by software */
#define PWRCTL_MPSM_EN                 BIT_32(4)
#define PWRCTL_EN_DFI_DRAM_CLK_DISABLE BIT_32(3)
#define PWRCTL_DEEPPOWERDOWN_EN        BIT_32(2)
#define PWRCTL_POWERDOWN_EN            BIT_32(1)
#define PWRCTL_SELFREF_EN              BIT_32(0)

/* PWRTMG */
#define PWRTMG_SELFREF_TO_X32_MASK     GENMASK_32(23, 16)
#define PWRTMG_SELFREF_TO_X32_SHIFT    16
#define PWRTMG_T_DPD_X4096_MASK        GENMASK_32(15, 8)
#define PWRTMG_T_DPD_X4096_SHIFT       8
#define PWRTMG_POWERDOWN_TO_X32_MASK   GENMASK_32(4, 0)
#define PWRTMG_POWERDOWN_TO_X32_SHIFT  0

/* RFSHCTL0 */
#define RFSHCTL0_REFRESH_MARGIN_MASK   GENMASK_32(23, 20)
#define RFSHCTL0_REFRESH_MARGIN_SHIFT  20
#define RFSHCTL0_REFRESH_TO_X32_MASK   GENMASK_32(16, 12)
#define RFSHCTL0_REFRESH_TO_X32_SHIFT  12
#define RFSHCTL0_REFRESH_BURST_MASK    GENMASK_32(8, 4)
#define RFSHCTL0_REFRESH_BURST_SHIFT   4
#define RFSHCTL0_PER_BANK_REFRESH      BIT_32(2)

/* RFSHCTL3 */
#define RFSHCTL3_REFRESH_MODE_MASK     GENMASK_32(6, 4)
#define RFSHCTL3_REFRESH_MODE_SHIFT    4
#define RFSHCTL3_REFRESH_UPDATE_LEVEL  BIT_32(1)
#define RFSHCTL3_DIS_AUTO_REFRESH      BIT_32(0)

/* RFSHTMG */
#define RFSHTMG_T_RFC_NOM_X32_MASK     GENMASK_32(27, 16)
#define RFSHTMG_T_RFC_NOM_X32_SHIFT    16
#define RFSHTMG_T_RFC_MIN_MASK         GENMASK_32(9, 0)
#define RFSHTMG_T_RFC_MIN_SHIFT        0

/* ECCCFG0 */
#define ECCCFG0_DIS_SCRUB              BIT_32(4)
#define ECCCFG0_ECC_MODE_MASK          GENMASK_32(2, 0) /* 0: off, 4: SEC/DED */
#define ECCCFG0_ECC_MODE_SHIFT         0

/* INIT0 */
#define INIT0_SKIP_DRAM_INIT_MASK      GENMASK_32(31, 30)
#define INIT0_SKIP_DRAM_INIT_SHIFT     30
#define INIT0_POST_CKE_X1024_MASK      GENMASK_32(25, 16)
#define INIT0_POST_CKE_X1024_SHIFT     16
#define INIT0_PRE_CKE_X1024_MASK       GENMASK_32(11, 0)
#define INIT0_PRE_CKE_X1024_SHIFT      0

/* INIT1 */
#define INIT1_DRAM_RSTN_X1024_MASK     GENMASK_32(24, 16)
#define INIT1_DRAM_RSTN_X1024_SHIFT    16
#define INIT1_FINAL_WAIT_X32_MASK      GENMASK_32(14, 8)
#define INIT1_FINAL_WAIT_X32_SHIFT     8
#define INIT1_PRE_OCD_X32_MASK         GENMASK_32(3, 0)
#define INIT1_PRE_OCD_X32_SHIFT        0

/*
 * INIT3/4/6/7: the mode register values the controller writes during SDRAM
 * init, two 16 bit values per register. With LPDDR4 [i.MX8M]:
 *   INIT3: MR1 / MR2     INIT4: MR3 / MR13
 *   INIT6: MR11 / MR12   INIT7: MR22 / MR14
 */
#define INIT_MR_HI_MASK                GENMASK_32(31, 16)
#define INIT_MR_HI_SHIFT               16
#define INIT_MR_LO_MASK                GENMASK_32(15, 0)
#define INIT_MR_LO_SHIFT               0

/* RANKCTL */
#define RANKCTL_DIFF_RANK_WR_GAP_MASK  GENMASK_32(11, 8)
#define RANKCTL_DIFF_RANK_WR_GAP_SHIFT 8
#define RANKCTL_DIFF_RANK_RD_GAP_MASK  GENMASK_32(7, 4)
#define RANKCTL_DIFF_RANK_RD_GAP_SHIFT 4
#define RANKCTL_MAX_RANK_RD_MASK       GENMASK_32(3, 0)
#define RANKCTL_MAX_RANK_RD_SHIFT      0

/* DRAMTMG0 */
#define DRAMTMG0_WR2PRE_MASK           GENMASK_32(30, 24)
#define DRAMTMG0_WR2PRE_SHIFT          24
#define DRAMTMG0_T_FAW_MASK            GENMASK_32(21, 16)
#define DRAMTMG0_T_FAW_SHIFT           16
#define DRAMTMG0_T_RAS_MAX_MASK        GENMASK_32(14, 8)
#define DRAMTMG0_T_RAS_MAX_SHIFT       8
#define DRAMTMG0_T_RAS_MIN_MASK        GENMASK_32(5, 0)
#define DRAMTMG0_T_RAS_MIN_SHIFT       0

/* DRAMTMG1 */
#define DRAMTMG1_T_XP_MASK             GENMASK_32(20, 16)
#define DRAMTMG1_T_XP_SHIFT            16
#define DRAMTMG1_RD2PRE_MASK           GENMASK_32(12, 8)
#define DRAMTMG1_RD2PRE_SHIFT          8
#define DRAMTMG1_T_RC_MASK             GENMASK_32(6, 0)
#define DRAMTMG1_T_RC_SHIFT            0

/* DRAMTMG2 */
#define DRAMTMG2_WRITE_LATENCY_MASK    GENMASK_32(29, 24)
#define DRAMTMG2_WRITE_LATENCY_SHIFT   24
#define DRAMTMG2_READ_LATENCY_MASK     GENMASK_32(21, 16)
#define DRAMTMG2_READ_LATENCY_SHIFT    16
#define DRAMTMG2_RD2WR_MASK            GENMASK_32(13, 8)
#define DRAMTMG2_RD2WR_SHIFT           8
#define DRAMTMG2_WR2RD_MASK            GENMASK_32(5, 0)
#define DRAMTMG2_WR2RD_SHIFT           0

/* DRAMTMG3 */
#define DRAMTMG3_T_MRW_MASK            GENMASK_32(29, 20)
#define DRAMTMG3_T_MRW_SHIFT           20
#define DRAMTMG3_T_MRD_MASK            GENMASK_32(17, 12)
#define DRAMTMG3_T_MRD_SHIFT           12
#define DRAMTMG3_T_MOD_MASK            GENMASK_32(9, 0)
#define DRAMTMG3_T_MOD_SHIFT           0

/* DRAMTMG4 */
#define DRAMTMG4_T_RCD_MASK            GENMASK_32(28, 24)
#define DRAMTMG4_T_RCD_SHIFT           24
#define DRAMTMG4_T_CCD_MASK            GENMASK_32(19, 16)
#define DRAMTMG4_T_CCD_SHIFT           16
#define DRAMTMG4_T_RRD_MASK            GENMASK_32(11, 8)
#define DRAMTMG4_T_RRD_SHIFT           8
#define DRAMTMG4_T_RP_MASK             GENMASK_32(4, 0)
#define DRAMTMG4_T_RP_SHIFT            0

/* DRAMTMG5 */
#define DRAMTMG5_T_CKSRX_MASK          GENMASK_32(27, 24)
#define DRAMTMG5_T_CKSRX_SHIFT         24
#define DRAMTMG5_T_CKSRE_MASK          GENMASK_32(19, 16)
#define DRAMTMG5_T_CKSRE_SHIFT         16
#define DRAMTMG5_T_CKESR_MASK          GENMASK_32(13, 8)
#define DRAMTMG5_T_CKESR_SHIFT         8
#define DRAMTMG5_T_CKE_MASK            GENMASK_32(4, 0)
#define DRAMTMG5_T_CKE_SHIFT           0

/* ZQCTL0 */
#define ZQCTL0_DIS_AUTO_ZQ             BIT_32(31)
#define ZQCTL0_DIS_SRX_ZQCL            BIT_32(30)
#define ZQCTL0_ZQ_RESISTOR_SHARED      BIT_32(29)
#define ZQCTL0_DIS_MPSMX_ZQCL          BIT_32(28)
#define ZQCTL0_T_ZQ_LONG_NOP_MASK      GENMASK_32(26, 16)
#define ZQCTL0_T_ZQ_LONG_NOP_SHIFT     16
#define ZQCTL0_T_ZQ_SHORT_NOP_MASK     GENMASK_32(9, 0)
#define ZQCTL0_T_ZQ_SHORT_NOP_SHIFT    0

/* ZQCTL1 */
#define ZQCTL1_T_ZQ_RESET_NOP_MASK     GENMASK_32(29, 20)
#define ZQCTL1_T_ZQ_RESET_NOP_SHIFT    20
#define ZQCTL1_T_ZQ_SHORT_INTERVAL_X1024_MASK  GENMASK_32(19, 0)
#define ZQCTL1_T_ZQ_SHORT_INTERVAL_X1024_SHIFT 0

/* DFIMISC */
#define DFIMISC_DFI_FREQUENCY_MASK             GENMASK_32(12, 8) /* [i.MX8M] */
#define DFIMISC_DFI_FREQUENCY_SHIFT            8
#define DFIMISC_DFI_INIT_START                 BIT_32(5) /* [i.MX8M] */
#define DFIMISC_CTL_IDLE_EN                    BIT_32(4) /* [i.MX8M] */
#define DFIMISC_DFI_DATA_CS_POLARITY           BIT_32(2)
#define DFIMISC_PHY_DBI_MODE                   BIT_32(1)
#define DFIMISC_DFI_INIT_COMPLETE_EN           BIT_32(0)

/* DFISTAT [i.MX8M] */
#define DFISTAT_DFI_LP_ACK                     BIT_32(1)
#define DFISTAT_DFI_INIT_COMPLETE              BIT_32(0)

/* DBICTL */
#define DBICTL_RD_DBI_EN                       BIT_32(2)
#define DBICTL_WR_DBI_EN                       BIT_32(1)
#define DBICTL_DM_EN                           BIT_32(0)

/* DFIPHYMSTR [i.MX8M] */
#define DFIPHYMSTR_DFI_PHYMSTR_EN              BIT_32(0) /* Allow PHY initiated updates */

/* ODTCFG */
#define ODTCFG_WR_ODT_HOLD_MASK                GENMASK_32(27, 24)
#define ODTCFG_WR_ODT_HOLD_SHIFT               24
#define ODTCFG_WR_ODT_DELAY_MASK               GENMASK_32(20, 16)
#define ODTCFG_WR_ODT_DELAY_SHIFT              16
#define ODTCFG_RD_ODT_HOLD_MASK                GENMASK_32(11, 8)
#define ODTCFG_RD_ODT_HOLD_SHIFT               8
#define ODTCFG_RD_ODT_DELAY_MASK               GENMASK_32(6, 2)
#define ODTCFG_RD_ODT_DELAY_SHIFT              2

/* ODTMAP */
#define ODTMAP_RANK1_WR_ODT_MASK               GENMASK_32(9, 8)
#define ODTMAP_RANK1_WR_ODT_SHIFT              8
#define ODTMAP_RANK0_WR_ODT_MASK               GENMASK_32(1, 0)
#define ODTMAP_RANK0_WR_ODT_SHIFT              0

/* SCHED */
#define SCHED_GO2CRITICAL_HYSTERESIS_MASK      GENMASK_32(23, 16)
#define SCHED_GO2CRITICAL_HYSTERESIS_SHIFT     16
#define SCHED_LPR_NUM_ENTRIES_MASK             GENMASK_32(13, 8)
#define SCHED_LPR_NUM_ENTRIES_SHIFT            8
#define SCHED_PREFER_WRITE                     BIT_32(1)
#define SCHED_FORCE_LOW_PRI_N                  BIT_32(0)

/* DBG0 */
#define DBG0_DIS_COLLISION_PAGE_OPT            BIT_32(4)
#define DBG0_DIS_WC                            BIT_32(0)

/* DBG1 */
#define DBG1_DIS_HIF                           BIT_32(1) /* No new commands from the ports */
#define DBG1_DIS_DQ                            BIT_32(0) /* No reads or writes to the SDRAM */

/* DBGCMD */
#define DBGCMD_CTRLUPD                         BIT_32(5)
#define DBGCMD_ZQ_CALIB_SHORT                  BIT_32(4)
#define DBGCMD_RANK1_REFRESH                   BIT_32(1)
#define DBGCMD_RANK0_REFRESH                   BIT_32(0)

/*
 * SWCTL/SWSTAT: quasi-dynamic registers may only be written while
 * SWCTL.sw_done is 0. Setting it back to 1 applies them, and SWSTAT.
 * sw_done_ack says when that is done.
 */
#define SWCTL_SW_DONE                          BIT_32(0)
#define SWSTAT_SW_DONE_ACK                     BIT_32(0)

/* PCCFG */
#define PCCFG_BL_EXP_MODE                      BIT_32(8)
#define PCCFG_PAGEMATCH_LIMIT                  BIT_32(4)
#define PCCFG_GO2CRITICAL_EN                   BIT_32(0) /* [i.MX8M] */

/* PCTRL_n */
#define PCTRL_PORT_EN                          BIT_32(0)

#endif // PLAT_IMX8M_UMCTL2_H_
