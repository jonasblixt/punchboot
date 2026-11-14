#ifndef INCLUDE_PLAT_IMX8M_FUSEBOX_H
#define INCLUDE_PLAT_IMX8M_FUSEBOX_H

/* The OCOTP has 4 words per bank */
#define IMX8M_FUSE_WORDS_PER_BANK (4)

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
#define IMX8MQ_FUSE_REVOKE_BANK (9)
#define IMX8MQ_FUSE_REVOKE_WORD (3)
#endif

#endif // INCLUDE_PLAT_IMX8M_FUSEBOX_H
