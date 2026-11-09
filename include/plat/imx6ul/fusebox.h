#ifndef INCLUDE_PLAT_IMX6UL_FUSEBOX_H
#define INCLUDE_PLAT_IMX6UL_FUSEBOX_H

#include <pb/utils_def.h>
#include <stdint.h>

/* The OCOTP has 8 words per bank. A fuse's index, for slc_fuse */
#define IMX6UL_FUSE_WORDS_PER_BANK      (8)
#define IMX6UL_FUSE(bank, word)         ((bank) * IMX6UL_FUSE_WORDS_PER_BANK + (word))

#define IMX6UL_FUSE_UNIQUE_BANK         0
#define IMX6UL_FUSE_UNIQUE1_WORD        1
#define IMX6UL_FUSE_UNIQUE2_WORD        2

/* 0x430[19:16] SI_REV and 0x440[17:16] SPEED_GRADING, bank 0 (RM Table 5-9) */
#define IMX6UL_FUSE_SI_REV_WORD         (3)
#define IMX6UL_FUSE_SI_REV_SHIFT        (16)
#define IMX6UL_FUSE_SI_REV_MASK         (0xf)
#define IMX6UL_FUSE_SPEED_GRADE_WORD    (4)
#define IMX6UL_FUSE_SPEED_GRADE_SHIFT   (16)
#define IMX6UL_FUSE_SPEED_GRADE_MASK    (0x3)

/*
 * BOOT_CFG1..4 (0x450), here for the uSDHC boot (RM Table 5-6 and 8-15).
 * SRC_SBMR1 has the same layout: the fuses or the boot pins the ROM used.
 */
#define IMX6UL_FUSE_BOOT_CFG_BANK       (0)
#define IMX6UL_FUSE_BOOT_CFG_WORD       (5)
#define IMX6UL_BOOT_CFG_USDHC           (1 << 6) /* BOOT_CFG1[7:6] 01: uSDHC */
#define IMX6UL_BOOT_CFG_MMC             BIT(5) /* 1: MMC/eMMC, 0: SD */
#define IMX6UL_BOOT_CFG_FAST_BOOT       BIT(4)
#define IMX6UL_BOOT_CFG_MMC_HIGH_SPEED  BIT(3) /* Table 8-15; Table 5-6 has it the other way */
#define IMX6UL_BOOT_CFG_USDHC_PORT(n)   ((n) << 11) /* BOOT_CFG2[4:3]: 0 uSDHC1, 1 uSDHC2 */
#define IMX6UL_BOOT_CFG_BUS_WIDTH(x)    ((x) << 13) /* BOOT_CFG2[7:5] */
#define IMX6UL_BOOT_CFG_BUS_WIDTH_OF(c) (((c) >> 13) & 0x7)
#define IMX6UL_BOOT_BUS_1BIT            (0)
#define IMX6UL_BOOT_BUS_4BIT            (1)
#define IMX6UL_BOOT_BUS_8BIT            (2)
#define IMX6UL_BOOT_BUS_4BIT_DDR        (5)
#define IMX6UL_BOOT_BUS_8BIT_DDR        (6)

/* 0x460: SEC_CONFIG[1], the closed life cycle, and BT_FUSE_SEL (RM Table 5-9) */
#define IMX6UL_FUSE_SEC_CONFIG_BANK     (0)
#define IMX6UL_FUSE_SEC_CONFIG_WORD     (6)
#define IMX6UL_FUSE_SEC_CONFIG_CLOSED   BIT(1)
#define IMX6UL_FUSE_BT_FUSE_SEL         BIT(4) /* Boot from the BOOT_CFG fuses, not the pins */

/* SRK_HASH (0x580..0x5F0) and SRK_REVOKE[2:0] (0x6F0): HAB revokes SRK0-2 */
#define IMX6UL_FUSE_SRK_BANK            (3)
#define IMX6UL_FUSE_SRK_WORD            (0)
#define IMX6UL_FUSE_REVOKE_BANK         (5)
#define IMX6UL_FUSE_REVOKE_WORD         (7)
#define IMX6UL_FUSE_REVOCABLE_SRKS      (3)

#endif // INCLUDE_PLAT_IMX6UL_FUSEBOX_H
