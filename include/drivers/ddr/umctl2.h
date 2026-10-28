/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Synopsys DesignWare uMCTL2 DDR memory controller.
 *
 * Used on the NXP i.MX8M family and, at the same offsets, as the PS DDR
 * controller of the AMD/Xilinx Zynq UltraScale+ MPSoC/RFSoC ("DDRC" at
 * 0xFD070000 in UG1087, "xlnx,zynqmp-ddrc-2.40a") and the Allwinner
 * H6/H616. The offsets and the fields below match the ZynqMP register map.
 * Fields marked [i.MX8M] are not programmed by the ZynqMP FSBL and come
 * from the i.MX8M reference manual and NXP's reference code.
 *
 * UMCTL2_<register> is the offset from the controller base. Register and
 * field names follow the Synopsys databook, as used by both vendors.
 */

#ifndef INCLUDE_DRIVERS_DDR_UMCTL2_H
#define INCLUDE_DRIVERS_DDR_UMCTL2_H

#include <drivers/ddr/dram_cfg.h>
#include <pb/utils_def.h>
#include <stdint.h>

/* Insert value 'v' into the field 'f' (f##_MASK, f##_SHIFT) */
#define UMCTL2_VAL(f, v)                       ((((uint32_t)(v)) << f##_SHIFT) & f##_MASK)
#define UMCTL2_GET(f, reg)                     ((((uint32_t)(reg)) & f##_MASK) >> f##_SHIFT)

/* Register map, offsets from the controller base ------------------------ */

#define UMCTL2_MSTR                            0x000 /* Master */
#define UMCTL2_STAT                            0x004 /* Operating mode status */
#define UMCTL2_MSTR1                           0x008
#define UMCTL2_MRCTRL0                         0x010 /* Mode register read/write control */
#define UMCTL2_MRCTRL1                         0x014
#define UMCTL2_MRSTAT                          0x018
#define UMCTL2_MRCTRL2                         0x01c
#define UMCTL2_DERATEEN                        0x020 /* LPDDR temperature derating */
#define UMCTL2_DERATEINT                       0x024
#define UMCTL2_MSTR2                           0x028 /* Target frequency (FREQ0..3) */
#define UMCTL2_PWRCTL                          0x030 /* Low power control */
#define UMCTL2_PWRTMG                          0x034
#define UMCTL2_HWLPCTL                         0x038
#define UMCTL2_HWFFCCTL                        0x03c /* Hardware fast frequency change */
#define UMCTL2_HWFFCSTAT                       0x040
#define UMCTL2_RFSHCTL0                        0x050 /* Refresh control */
#define UMCTL2_RFSHCTL1                        0x054
#define UMCTL2_RFSHCTL2                        0x058
#define UMCTL2_RFSHCTL3                        0x060
#define UMCTL2_RFSHTMG                         0x064
#define UMCTL2_ECCCFG0                         0x070 /* ECC */
#define UMCTL2_ECCCFG1                         0x074
#define UMCTL2_ECCSTAT                         0x078
#define UMCTL2_ECCCLR                          0x07c
#define UMCTL2_ECCERRCNT                       0x080
#define UMCTL2_ECCCADDR0                       0x084
#define UMCTL2_ECCCADDR1                       0x088
#define UMCTL2_ECCCSYN0                        0x08c
#define UMCTL2_ECCCSYN1                        0x090
#define UMCTL2_ECCCSYN2                        0x094
#define UMCTL2_ECCBITMASK0                     0x098
#define UMCTL2_ECCBITMASK1                     0x09c
#define UMCTL2_ECCBITMASK2                     0x0a0
#define UMCTL2_ECCUADDR0                       0x0a4
#define UMCTL2_ECCUADDR1                       0x0a8
#define UMCTL2_ECCUSYN0                        0x0ac
#define UMCTL2_ECCUSYN1                        0x0b0
#define UMCTL2_ECCUSYN2                        0x0b4
#define UMCTL2_ECCPOISONADDR0                  0x0b8
#define UMCTL2_ECCPOISONADDR1                  0x0bc
#define UMCTL2_CRCPARCTL0                      0x0c0 /* DDR4 CRC/parity */
#define UMCTL2_CRCPARCTL1                      0x0c4
#define UMCTL2_CRCPARCTL2                      0x0c8
#define UMCTL2_CRCPARSTAT                      0x0cc
#define UMCTL2_INIT0                           0x0d0 /* SDRAM initialization */
#define UMCTL2_INIT1                           0x0d4
#define UMCTL2_INIT2                           0x0d8
#define UMCTL2_INIT3                           0x0dc
#define UMCTL2_INIT4                           0x0e0
#define UMCTL2_INIT5                           0x0e4
#define UMCTL2_INIT6                           0x0e8
#define UMCTL2_INIT7                           0x0ec
#define UMCTL2_DIMMCTL                         0x0f0
#define UMCTL2_RANKCTL                         0x0f4
#define UMCTL2_DRAMTMG0                        0x100 /* SDRAM timing */
#define UMCTL2_DRAMTMG1                        0x104
#define UMCTL2_DRAMTMG2                        0x108
#define UMCTL2_DRAMTMG3                        0x10c
#define UMCTL2_DRAMTMG4                        0x110
#define UMCTL2_DRAMTMG5                        0x114
#define UMCTL2_DRAMTMG6                        0x118
#define UMCTL2_DRAMTMG7                        0x11c
#define UMCTL2_DRAMTMG8                        0x120
#define UMCTL2_DRAMTMG9                        0x124
#define UMCTL2_DRAMTMG10                       0x128
#define UMCTL2_DRAMTMG11                       0x12c
#define UMCTL2_DRAMTMG12                       0x130
#define UMCTL2_DRAMTMG13                       0x134
#define UMCTL2_DRAMTMG14                       0x138
#define UMCTL2_DRAMTMG15                       0x13c
#define UMCTL2_DRAMTMG16                       0x140
#define UMCTL2_DRAMTMG17                       0x144
#define UMCTL2_ZQCTL0                          0x180 /* ZQ calibration */
#define UMCTL2_ZQCTL1                          0x184
#define UMCTL2_ZQCTL2                          0x188
#define UMCTL2_ZQSTAT                          0x18c
#define UMCTL2_DFITMG0                         0x190 /* DFI (controller <-> PHY) timing */
#define UMCTL2_DFITMG1                         0x194
#define UMCTL2_DFILPCFG0                       0x198
#define UMCTL2_DFILPCFG1                       0x19c
#define UMCTL2_DFIUPD0                         0x1a0
#define UMCTL2_DFIUPD1                         0x1a4
#define UMCTL2_DFIUPD2                         0x1a8
#define UMCTL2_DFIMISC                         0x1b0
#define UMCTL2_DFITMG2                         0x1b4
#define UMCTL2_DFITMG3                         0x1b8
#define UMCTL2_DFISTAT                         0x1bc
#define UMCTL2_DBICTL                          0x1c0
#define UMCTL2_DFIPHYMSTR                      0x1c4
#define UMCTL2_TRAINCTL0                       0x1d0
#define UMCTL2_TRAINCTL1                       0x1d4
#define UMCTL2_TRAINCTL2                       0x1d8
#define UMCTL2_TRAINSTAT                       0x1dc
#define UMCTL2_ADDRMAP0                        0x200 /* HIF address -> SDRAM address */
#define UMCTL2_ADDRMAP1                        0x204
#define UMCTL2_ADDRMAP2                        0x208
#define UMCTL2_ADDRMAP3                        0x20c
#define UMCTL2_ADDRMAP4                        0x210
#define UMCTL2_ADDRMAP5                        0x214
#define UMCTL2_ADDRMAP6                        0x218
#define UMCTL2_ADDRMAP7                        0x21c
#define UMCTL2_ADDRMAP8                        0x220
#define UMCTL2_ADDRMAP9                        0x224
#define UMCTL2_ADDRMAP10                       0x228
#define UMCTL2_ADDRMAP11                       0x22c
#define UMCTL2_ODTCFG                          0x240
#define UMCTL2_ODTMAP                          0x244
#define UMCTL2_SCHED                           0x250 /* Scheduler */
#define UMCTL2_SCHED1                          0x254
#define UMCTL2_PERFHPR1                        0x25c
#define UMCTL2_PERFLPR1                        0x264
#define UMCTL2_PERFWR1                         0x26c
#define UMCTL2_PERFVPR1                        0x274
#define UMCTL2_PERFVPW1                        0x278
#define UMCTL2_DQMAP0                          0x280
#define UMCTL2_DQMAP1                          0x284
#define UMCTL2_DQMAP2                          0x288
#define UMCTL2_DQMAP3                          0x28c
#define UMCTL2_DQMAP4                          0x290
#define UMCTL2_DQMAP5                          0x294
#define UMCTL2_DBG0                            0x300 /* Debug */
#define UMCTL2_DBG1                            0x304
#define UMCTL2_DBGCAM                          0x308
#define UMCTL2_DBGCMD                          0x30c
#define UMCTL2_DBGSTAT                         0x310
#define UMCTL2_SWCTL                           0x320 /* Quasi-dynamic register programming */
#define UMCTL2_SWSTAT                          0x324
#define UMCTL2_OCPARCFG0                       0x330 /* On-chip parity */
#define UMCTL2_OCPARCFG1                       0x334
#define UMCTL2_OCPARCFG2                       0x338
#define UMCTL2_OCPARCFG3                       0x33c
#define UMCTL2_OCPARSTAT0                      0x340
#define UMCTL2_OCPARSTAT1                      0x344
#define UMCTL2_OCPARWLOG0                      0x348
#define UMCTL2_OCPARWLOG1                      0x34c
#define UMCTL2_OCPARWLOG2                      0x350
#define UMCTL2_OCPARAWLOG0                     0x354
#define UMCTL2_OCPARAWLOG1                     0x358
#define UMCTL2_OCPARRLOG0                      0x35c
#define UMCTL2_OCPARRLOG1                      0x360
#define UMCTL2_OCPARARLOG0                     0x364
#define UMCTL2_OCPARARLOG1                     0x368
#define UMCTL2_POISONCFG                       0x36c
#define UMCTL2_POISONSTAT                      0x370
#define UMCTL2_PSTAT                           0x3fc
#define UMCTL2_PCCFG                           0x400
#define UMCTL2_PCFGIDMASKCH                    0x410
#define UMCTL2_PCFGIDVALUECH                   0x414
#define UMCTL2_SARBASE0                        0xf04 /* System address regions */
#define UMCTL2_SARSIZE0                        0xf08
#define UMCTL2_SBRCTL                          0xf24 /* Scrubber */
#define UMCTL2_SBRSTAT                         0xf28
#define UMCTL2_SBRWDATA0                       0xf2c
#define UMCTL2_SBRWDATA1                       0xf30
#define UMCTL2_PDCH                            0xf34

/*
 * Multiport (AXI) registers. Port n's registers are at 0xb0 * n from
 * port 0's (PCTRL_1 is at 0x540 on both the i.MX8M and the ZynqMP).
 */
#define UMCTL2_PORT_STRIDE                     0xb0
#define UMCTL2_PCFGR(n)                        (0x404 + (n) * UMCTL2_PORT_STRIDE)
#define UMCTL2_PCFGW(n)                        (0x408 + (n) * UMCTL2_PORT_STRIDE)
#define UMCTL2_PCFGC(n)                        (0x40c + (n) * UMCTL2_PORT_STRIDE)
#define UMCTL2_PCTRL(n)                        (0x490 + (n) * UMCTL2_PORT_STRIDE)
#define UMCTL2_PCFGQOS0(n)                     (0x494 + (n) * UMCTL2_PORT_STRIDE)
#define UMCTL2_PCFGQOS1(n)                     (0x498 + (n) * UMCTL2_PORT_STRIDE)
#define UMCTL2_PCFGWQOS0(n)                    (0x49c + (n) * UMCTL2_PORT_STRIDE)
#define UMCTL2_PCFGWQOS1(n)                    (0x4a0 + (n) * UMCTL2_PORT_STRIDE)

/*
 * Frequency set point shadow registers. The timing registers of FREQ1..3
 * (selected by MSTR2.target_frequency) are copies of the FREQ0 registers at
 * 0x1000 * (n + 1). Only these registers have shadows:
 */
#define UMCTL2_FREQ(n, off)                    ((off) + 0x1000 * ((n) + 1))
/*
 * DERATEEN, DERATEINT, RFSHCTL0, RFSHTMG, INIT3, INIT4, INIT6, INIT7,
 * DRAMTMG0, DRAMTMG1, DRAMTMG2, DRAMTMG3, DRAMTMG4, DRAMTMG5, DRAMTMG6,
 * DRAMTMG7, DRAMTMG8, DRAMTMG9, DRAMTMG10, DRAMTMG11, DRAMTMG12,
 * DRAMTMG13, DRAMTMG14, DRAMTMG15, DRAMTMG16, DRAMTMG17, ZQCTL0, DFITMG0,
 * DFITMG1, DFITMG2, DFITMG3, ODTCFG
 */

/* Register fields ------------------------------------------------------- */

/* MSTR */
#define MSTR_DEVICE_CONFIG_MASK                GENMASK_32(31, 30) /* 0: x4, 1: x8, 2: x16, 3: x32 */
#define MSTR_DEVICE_CONFIG_SHIFT               30
#define MSTR_FREQUENCY_MODE                    BIT_32(29) /* Enable FREQ1..3 (MSTR2) */
#define MSTR_ACTIVE_RANKS_MASK                 GENMASK_32(25, 24) /* 1: rank 0, 3: ranks 0 and 1 */
#define MSTR_ACTIVE_RANKS_SHIFT                24
#define MSTR_BURST_RDWR_MASK                   GENMASK_32(19, 16) /* Burst length / 2 */
#define MSTR_BURST_RDWR_SHIFT                  16
#define MSTR_DLL_OFF_MODE                      BIT_32(15)
#define MSTR_DATA_BUS_WIDTH_MASK               GENMASK_32(13, 12) /* 0: full, 1: half, 2: quarter */
#define MSTR_DATA_BUS_WIDTH_SHIFT              12
#define MSTR_GEARDOWN_MODE                     BIT_32(11)
#define MSTR_EN_2T_TIMING_MODE                 BIT_32(10)
#define MSTR_BURSTCHOP                         BIT_32(9)
#define MSTR_LPDDR4                            BIT_32(5)
#define MSTR_DDR4                              BIT_32(4)
#define MSTR_LPDDR3                            BIT_32(3)
#define MSTR_LPDDR2                            BIT_32(2)
#define MSTR_DDR3                              BIT_32(0)

/* STAT [i.MX8M] */
#define STAT_SELFREF_STATE_MASK                GENMASK_32(9, 8)
#define STAT_SELFREF_STATE_SHIFT               8
#define STAT_SELFREF_TYPE_MASK                 GENMASK_32(5, 4)
#define STAT_SELFREF_TYPE_SHIFT                4
#define STAT_OPERATING_MODE_MASK               GENMASK_32(2, 0)
#define STAT_OPERATING_MODE_SHIFT              0
#define STAT_OPERATING_MODE_NORMAL             1

/* MRCTRL0 */
#define MRCTRL0_MR_WR                          BIT_32(31)
#define MRCTRL0_MR_ADDR_MASK                   GENMASK_32(15, 12)
#define MRCTRL0_MR_ADDR_SHIFT                  12
#define MRCTRL0_MR_RANK_MASK                   GENMASK_32(5, 4)
#define MRCTRL0_MR_RANK_SHIFT                  4
#define MRCTRL0_PDA_EN                         BIT_32(2)
#define MRCTRL0_MPR_EN                         BIT_32(1)
#define MRCTRL0_MR_TYPE                        BIT_32(0) /* 0: write, 1: read */

/* DERATEEN */
#define DERATEEN_RC_DERATE_VALUE_MASK          GENMASK_32(9, 8)
#define DERATEEN_RC_DERATE_VALUE_SHIFT         8
#define DERATEEN_DERATE_BYTE_MASK              GENMASK_32(7, 4)
#define DERATEEN_DERATE_BYTE_SHIFT             4
#define DERATEEN_DERATE_VALUE                  BIT_32(1)
#define DERATEEN_DERATE_ENABLE                 BIT_32(0)

/* MSTR2 */
#define MSTR2_TARGET_FREQUENCY_MASK            GENMASK_32(1, 0)
#define MSTR2_TARGET_FREQUENCY_SHIFT           0

/* PWRCTL */
#define PWRCTL_DIS_CAM_DRAIN_SELFREF           BIT_32(7) /* [i.MX8M] */
#define PWRCTL_STAY_IN_SELFREF                 BIT_32(6)
#define PWRCTL_SELFREF_SW                      BIT_32(5) /* Enter self refresh by software */
#define PWRCTL_MPSM_EN                         BIT_32(4)
#define PWRCTL_EN_DFI_DRAM_CLK_DISABLE         BIT_32(3)
#define PWRCTL_DEEPPOWERDOWN_EN                BIT_32(2)
#define PWRCTL_POWERDOWN_EN                    BIT_32(1)
#define PWRCTL_SELFREF_EN                      BIT_32(0)

/* PWRTMG */
#define PWRTMG_SELFREF_TO_X32_MASK             GENMASK_32(23, 16)
#define PWRTMG_SELFREF_TO_X32_SHIFT            16
#define PWRTMG_T_DPD_X4096_MASK                GENMASK_32(15, 8)
#define PWRTMG_T_DPD_X4096_SHIFT               8
#define PWRTMG_POWERDOWN_TO_X32_MASK           GENMASK_32(4, 0)
#define PWRTMG_POWERDOWN_TO_X32_SHIFT          0

/* RFSHCTL0 */
#define RFSHCTL0_REFRESH_MARGIN_MASK           GENMASK_32(23, 20)
#define RFSHCTL0_REFRESH_MARGIN_SHIFT          20
#define RFSHCTL0_REFRESH_TO_X32_MASK           GENMASK_32(16, 12)
#define RFSHCTL0_REFRESH_TO_X32_SHIFT          12
#define RFSHCTL0_REFRESH_BURST_MASK            GENMASK_32(8, 4)
#define RFSHCTL0_REFRESH_BURST_SHIFT           4
#define RFSHCTL0_PER_BANK_REFRESH              BIT_32(2)

/* RFSHCTL3 */
#define RFSHCTL3_REFRESH_MODE_MASK             GENMASK_32(6, 4)
#define RFSHCTL3_REFRESH_MODE_SHIFT            4
#define RFSHCTL3_REFRESH_UPDATE_LEVEL          BIT_32(1)
#define RFSHCTL3_DIS_AUTO_REFRESH              BIT_32(0)

/* RFSHTMG */
#define RFSHTMG_T_RFC_NOM_X32_MASK             GENMASK_32(27, 16)
#define RFSHTMG_T_RFC_NOM_X32_SHIFT            16
#define RFSHTMG_T_RFC_MIN_MASK                 GENMASK_32(9, 0)
#define RFSHTMG_T_RFC_MIN_SHIFT                0

/* ECCCFG0 */
#define ECCCFG0_DIS_SCRUB                      BIT_32(4)
#define ECCCFG0_ECC_MODE_MASK                  GENMASK_32(2, 0) /* 0: off, 4: SEC/DED */
#define ECCCFG0_ECC_MODE_SHIFT                 0

/* INIT0 */
#define INIT0_SKIP_DRAM_INIT_MASK              GENMASK_32(31, 30)
#define INIT0_SKIP_DRAM_INIT_SHIFT             30
#define INIT0_POST_CKE_X1024_MASK              GENMASK_32(25, 16)
#define INIT0_POST_CKE_X1024_SHIFT             16
#define INIT0_PRE_CKE_X1024_MASK               GENMASK_32(11, 0)
#define INIT0_PRE_CKE_X1024_SHIFT              0

/* INIT1 */
#define INIT1_DRAM_RSTN_X1024_MASK             GENMASK_32(24, 16)
#define INIT1_DRAM_RSTN_X1024_SHIFT            16
#define INIT1_FINAL_WAIT_X32_MASK              GENMASK_32(14, 8)
#define INIT1_FINAL_WAIT_X32_SHIFT             8
#define INIT1_PRE_OCD_X32_MASK                 GENMASK_32(3, 0)
#define INIT1_PRE_OCD_X32_SHIFT                0

/*
 * INIT3/4/6/7: the mode register values the controller writes during SDRAM
 * init, two 16 bit values per register. With LPDDR4 [i.MX8M]:
 *   INIT3: MR1 / MR2     INIT4: MR3 / MR13
 *   INIT6: MR11 / MR12   INIT7: MR22 / MR14
 */
#define INIT_MR_HI_MASK                        GENMASK_32(31, 16)
#define INIT_MR_HI_SHIFT                       16
#define INIT_MR_LO_MASK                        GENMASK_32(15, 0)
#define INIT_MR_LO_SHIFT                       0

/* RANKCTL */
#define RANKCTL_DIFF_RANK_WR_GAP_MASK          GENMASK_32(11, 8)
#define RANKCTL_DIFF_RANK_WR_GAP_SHIFT         8
#define RANKCTL_DIFF_RANK_RD_GAP_MASK          GENMASK_32(7, 4)
#define RANKCTL_DIFF_RANK_RD_GAP_SHIFT         4
#define RANKCTL_MAX_RANK_RD_MASK               GENMASK_32(3, 0)
#define RANKCTL_MAX_RANK_RD_SHIFT              0

/* DRAMTMG0 */
#define DRAMTMG0_WR2PRE_MASK                   GENMASK_32(30, 24)
#define DRAMTMG0_WR2PRE_SHIFT                  24
#define DRAMTMG0_T_FAW_MASK                    GENMASK_32(21, 16)
#define DRAMTMG0_T_FAW_SHIFT                   16
#define DRAMTMG0_T_RAS_MAX_MASK                GENMASK_32(14, 8)
#define DRAMTMG0_T_RAS_MAX_SHIFT               8
#define DRAMTMG0_T_RAS_MIN_MASK                GENMASK_32(5, 0)
#define DRAMTMG0_T_RAS_MIN_SHIFT               0

/* DRAMTMG1 */
#define DRAMTMG1_T_XP_MASK                     GENMASK_32(20, 16)
#define DRAMTMG1_T_XP_SHIFT                    16
#define DRAMTMG1_RD2PRE_MASK                   GENMASK_32(12, 8)
#define DRAMTMG1_RD2PRE_SHIFT                  8
#define DRAMTMG1_T_RC_MASK                     GENMASK_32(6, 0)
#define DRAMTMG1_T_RC_SHIFT                    0

/* DRAMTMG2 */
#define DRAMTMG2_WRITE_LATENCY_MASK            GENMASK_32(29, 24)
#define DRAMTMG2_WRITE_LATENCY_SHIFT           24
#define DRAMTMG2_READ_LATENCY_MASK             GENMASK_32(21, 16)
#define DRAMTMG2_READ_LATENCY_SHIFT            16
#define DRAMTMG2_RD2WR_MASK                    GENMASK_32(13, 8)
#define DRAMTMG2_RD2WR_SHIFT                   8
#define DRAMTMG2_WR2RD_MASK                    GENMASK_32(5, 0)
#define DRAMTMG2_WR2RD_SHIFT                   0

/* DRAMTMG3 */
#define DRAMTMG3_T_MRW_MASK                    GENMASK_32(29, 20)
#define DRAMTMG3_T_MRW_SHIFT                   20
#define DRAMTMG3_T_MRD_MASK                    GENMASK_32(17, 12)
#define DRAMTMG3_T_MRD_SHIFT                   12
#define DRAMTMG3_T_MOD_MASK                    GENMASK_32(9, 0)
#define DRAMTMG3_T_MOD_SHIFT                   0

/* DRAMTMG4 */
#define DRAMTMG4_T_RCD_MASK                    GENMASK_32(28, 24)
#define DRAMTMG4_T_RCD_SHIFT                   24
#define DRAMTMG4_T_CCD_MASK                    GENMASK_32(19, 16)
#define DRAMTMG4_T_CCD_SHIFT                   16
#define DRAMTMG4_T_RRD_MASK                    GENMASK_32(11, 8)
#define DRAMTMG4_T_RRD_SHIFT                   8
#define DRAMTMG4_T_RP_MASK                     GENMASK_32(4, 0)
#define DRAMTMG4_T_RP_SHIFT                    0

/* DRAMTMG5 */
#define DRAMTMG5_T_CKSRX_MASK                  GENMASK_32(27, 24)
#define DRAMTMG5_T_CKSRX_SHIFT                 24
#define DRAMTMG5_T_CKSRE_MASK                  GENMASK_32(19, 16)
#define DRAMTMG5_T_CKSRE_SHIFT                 16
#define DRAMTMG5_T_CKESR_MASK                  GENMASK_32(13, 8)
#define DRAMTMG5_T_CKESR_SHIFT                 8
#define DRAMTMG5_T_CKE_MASK                    GENMASK_32(4, 0)
#define DRAMTMG5_T_CKE_SHIFT                   0

/* ZQCTL0 */
#define ZQCTL0_DIS_AUTO_ZQ                     BIT_32(31)
#define ZQCTL0_DIS_SRX_ZQCL                    BIT_32(30)
#define ZQCTL0_ZQ_RESISTOR_SHARED              BIT_32(29)
#define ZQCTL0_DIS_MPSMX_ZQCL                  BIT_32(28)
#define ZQCTL0_T_ZQ_LONG_NOP_MASK              GENMASK_32(26, 16)
#define ZQCTL0_T_ZQ_LONG_NOP_SHIFT             16
#define ZQCTL0_T_ZQ_SHORT_NOP_MASK             GENMASK_32(9, 0)
#define ZQCTL0_T_ZQ_SHORT_NOP_SHIFT            0

/* ZQCTL1 */
#define ZQCTL1_T_ZQ_RESET_NOP_MASK             GENMASK_32(29, 20)
#define ZQCTL1_T_ZQ_RESET_NOP_SHIFT            20
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

/* API ------------------------------------------------------------------- */

/* Write a table of (absolute address, value) pairs */
void umctl2_write_cfg(const struct dram_cfg_param *cfg, unsigned int n);

/* Write a quasi-dynamic register, see SWCTL above */
void umctl2_write_quasi_dynamic(uintptr_t base, uint32_t off, uint32_t val);

/* DFIMISC with dfi_frequency matching MSTR2.target_frequency, and ctl_idle_en */
uint32_t umctl2_dfimisc(uintptr_t base);

/*
 * Allow reads and writes, but put the (not yet initialized) SDRAM in self
 * refresh by software, with auto refresh off, and wait for it.
 */
void umctl2_enter_selfref_sw(uintptr_t base);

/* Start the DFI init (DFIMISC.dfi_init_start) and wait for it to complete */
void umctl2_dfi_init(uintptr_t base);

/*
 * After the DFI init: set dfi_init_complete_en, leave self refresh, allow
 * PHY initiated updates, wait for normal operation and enable auto refresh.
 */
void umctl2_dfi_init_done(uintptr_t base);

void umctl2_enable_port(uintptr_t base, unsigned int port);

#endif // INCLUDE_DRIVERS_DDR_UMCTL2_H
