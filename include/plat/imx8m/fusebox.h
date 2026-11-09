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

#endif // INCLUDE_PLAT_IMX8M_FUSEBOX_H
