#ifndef INCLUDE_PLAT_IMX8M_FUSEBOX_H
#define INCLUDE_PLAT_IMX8M_FUSEBOX_H

/* The OCOTP has 4 words per bank */
#define IMX8M_FUSE_WORDS_PER_BANK (4)

/*
 * The words pb's unique id is read from, bank 0: 0x410 and 0x420. On the
 * 8MQ they are UNIQUE_ID[63:0]; on the 8MP 0x410 is lock fuses and
 * UNIQUE_ID[63:0] is 0x420 and 0x430 (8MP RM Table 6-35), F-169.
 */
#define IMX8M_FUSE_UNIQUE_BANK    (0)
#define IMX8M_FUSE_UNIQUE1_WORD   (1)
#define IMX8M_FUSE_UNIQUE2_WORD   (2)

#endif // INCLUDE_PLAT_IMX8M_FUSEBOX_H
