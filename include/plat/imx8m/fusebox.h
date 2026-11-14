#ifndef INCLUDE_PLAT_IMX8M_FUSEBOX_H
#define INCLUDE_PLAT_IMX8M_FUSEBOX_H

#include <stdint.h>

/* The OCOTP has 4 words per bank */
#define IMX8M_FUSE_WORDS_PER_BANK (4)

/* A fuse word's index, as struct slc_fuse and imx_ocotp_fuse_read() take it */
#define IMX8M_FUSE(bank, word)    ((bank) * IMX8M_FUSE_WORDS_PER_BANK + (word))

/*
 * UNIQUE_ID[63:0], bank 0: 0x410 and 0x420 on the 8MQ, 0x420 and 0x430 on
 * the 8MP, where 0x410 is lock fuses (8MP RM Table 6-35, F-169)
 */
#define IMX8M_FUSE_UNIQUE_BANK    (0)
#if defined(CONFIG_PLAT_IMX8MP)
#define IMX8M_FUSE_UNIQUE1_WORD (2)
#define IMX8M_FUSE_UNIQUE2_WORD (3)
#else
#define IMX8M_FUSE_UNIQUE1_WORD (1)
#define IMX8M_FUSE_UNIQUE2_WORD (2)
#endif

/*
 * HABv4's fuses (plat-imx.md 4.1). The fuse at 0x400 + 0x10 * (bank * 4 +
 * word).
 *
 * SEC_CONFIG[1], the closed bit, 0x470 bit 25, and SRK_HASH[255:0], banks 6
 * and 7 (0x580 to 0x5f0): the 8MQ RM's fuse map (6.3.1), and NXP's U-Boot for
 * the whole i.MX8M family, the 8M Plus too (mx8m_secure_boot.txt: "fuse prog
 * 1 3 0x2000000", "fuse prog 6 0 ..."); the 8M Plus's public RM doesn't have
 * its security fuses.
 *
 * SRK_REVOKE[2:0], the 8MQ's only: bank 9 word 3, the 8MQ RM's OCOTP
 * register "Value of OTP Bank9 Word3 (SRK Revoke)" (6.3.5.40) and its boot
 * fuses (Table 6-2), bit n SRK n as on the 6UL. Its fuse map calls 0x670
 * reserved. Where the 8M Plus has it isn't public: pb doesn't revoke there.
 */
#define IMX8M_FUSE_SEC_CONFIG_BANK   (1)
#define IMX8M_FUSE_SEC_CONFIG_WORD   (3)
#define IMX8M_FUSE_SEC_CONFIG_CLOSED (1u << 25)
#define IMX8M_FUSE_SRK_BANK          (6)
#define IMX8M_FUSE_SRK_WORD          (0)
#ifdef CONFIG_PLAT_IMX8MQ
#define IMX8MQ_FUSE_REVOKE_BANK          (9)
#define IMX8MQ_FUSE_REVOKE_WORD          (3)

/*
 * The i.MX8MQ's boot configuration, 0x470 (bank 1 word 3, beside
 * SEC_CONFIG[1]): BOOT_CFG[15:0] and BT_FUSE_SEL (8MQ RM Table 6-45). The
 * fields of BOOT_CFG for MMC/eMMC: Tables 6-37 and 6-39.
 */
#define IMX8MQ_FUSE_BOOT_CFG_BANK        (1)
#define IMX8MQ_FUSE_BOOT_CFG_WORD        (3)
#define IMX8MQ_FUSE_BT_FUSE_SEL          (1u << 28) /* 1: the boot mode from the fuses */
#define IMX8MQ_BOOT_CFG_INFINITE_LOOP    (1u << 15) /* Debug only */
#define IMX8MQ_BOOT_CFG_DEVICE(x)        ((uint32_t)(x) << 12) /* [14:12] */
#define IMX8MQ_BOOT_DEVICE_SD            (1)
#define IMX8MQ_BOOT_DEVICE_MMC           (2) /* MMC/eMMC */
#define IMX8MQ_BOOT_DEVICE_NAND          (3)
#define IMX8MQ_BOOT_DEVICE_QSPI          (4)
#define IMX8MQ_BOOT_DEVICE_SPI_NOR       (6)
#define IMX8MQ_BOOT_CFG_USDHC_PORT(n)    ((uint32_t)(n) << 10) /* [11:10]: 0 SD1, 1 SD2 */
#define IMX8MQ_BOOT_CFG_POWER_CYCLE      (1u << 9)
#define IMX8MQ_BOOT_CFG_SD_LOOPBACK_CLK  (1u << 8) /* SDR50 and SDR104 */
#define IMX8MQ_BOOT_CFG_FAST_BOOT        (1u << 7)
#define IMX8MQ_BOOT_CFG_MMC_BUS_WIDTH(x) ((uint32_t)(x) << 4) /* [6:4] */
#define IMX8MQ_BOOT_MMC_BUS_1BIT         (0)
#define IMX8MQ_BOOT_MMC_BUS_4BIT         (1)
#define IMX8MQ_BOOT_MMC_BUS_8BIT         (2)
#define IMX8MQ_BOOT_MMC_BUS_4BIT_DDR     (5) /* MMC 4.4 */
#define IMX8MQ_BOOT_MMC_BUS_8BIT_DDR     (6) /* MMC 4.4 */
#define IMX8MQ_BOOT_CFG_MMC_SPEED(x)     ((uint32_t)(x) << 2) /* [3:2] */
#define IMX8MQ_BOOT_MMC_SPEED_NORMAL     (0)
#define IMX8MQ_BOOT_MMC_SPEED_HIGH       (1)
#define IMX8MQ_BOOT_CFG_USDHC1_1V8       (1u << 1) /* USDHC1's IO, 0: 3.3 V */
#define IMX8MQ_BOOT_CFG_USDHC2_1V8       (1u << 0) /* USDHC2's IO, 0: 3.3 V */
#endif

#endif // INCLUDE_PLAT_IMX8M_FUSEBOX_H
